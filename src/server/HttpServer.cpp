#include "server/HttpServer.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/event.h>
#endif

#include "handlers/ErrorHandler.h"
#include "http/HttpParser.h"

HttpServer::HttpServer(int port, std::size_t numThreads, std::string docRoot, bool useKqueue)
    : port_(port),
      docRoot_(std::move(docRoot)),
      useKqueue_(useKqueue),
      threadPool_(numThreads),
      fileCache_(1024),
      fileHandler_(docRoot_, &fileCache_) {}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    if (running_.exchange(true)) {
        return;
    }

    listenSocket_ = std::make_unique<Socket>();
    listenSocket_->setReuseAddr();
    listenSocket_->bind(port_);
    listenSocket_->listen(128);

    if (useKqueue_) {
#if !defined(__APPLE__)
        throw std::runtime_error("kqueue mode is only supported on macOS/BSD platforms");
#else
        listenSocket_->setNonBlocking();
        ioThread_ = std::thread([this]() { runKqueueLoop(); });
        logger_.log("Server started on port " + std::to_string(port_) + " (kqueue mode)");
#endif
        return;
    }

    acceptor_ = std::make_unique<Acceptor>(std::move(*listenSocket_), threadPool_,
                                           [this](Socket socket, std::string clientIp) {
                                               handleConnection(std::move(socket), std::move(clientIp));
                                           });
    listenSocket_.reset();
    acceptor_->start();
    logger_.log("Server started on port " + std::to_string(port_) + " (thread-pool mode)");
}

void HttpServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (acceptor_) {
        acceptor_->stop();
    }
    if (listenSocket_) {
        listenSocket_->shutdownReadWrite();
        listenSocket_->close();
    }
    if (ioThread_.joinable()) {
        ioThread_.join();
    }
    threadPool_.shutdown();
    logger_.log("Server stopped");
}

void HttpServer::runKqueueLoop() {
#if defined(__APPLE__)
    constexpr std::size_t kBufferSize = 8192;
    constexpr std::size_t kMaxRequestBytes = 10 * 1024 * 1024;
    constexpr auto kIdleTimeout = std::chrono::seconds(60);

    struct ConnectionState {
        Socket socket;
        std::string clientIp;
        std::string readBuffer;
        std::string writeBuffer;
        bool closeAfterWrite{false};
        std::chrono::steady_clock::time_point lastActive;
    };

    auto registerEvent = [](int kq, int fd, int16_t filter, uint16_t flags) {
        struct kevent ev;
        EV_SET(&ev, fd, filter, flags, 0, 0, nullptr);
        return ::kevent(kq, &ev, 1, nullptr, 0, nullptr);
    };

    int kq = ::kqueue();
    if (kq < 0) {
        logger_.error("kqueue() failed: " + std::string(std::strerror(errno)));
        running_.store(false);
        return;
    }

    if (registerEvent(kq, listenSocket_->getFd(), EVFILT_READ, EV_ADD | EV_ENABLE) < 0) {
        logger_.error("kevent register listen socket failed: " + std::string(std::strerror(errno)));
        ::close(kq);
        running_.store(false);
        return;
    }

    std::unordered_map<int, ConnectionState> connections;
    connections.reserve(2048);
    http::HttpParser parser;
    std::vector<char> ioBuffer(kBufferSize);

    auto closeConnection = [&](int fd) {
        const auto it = connections.find(fd);
        if (it == connections.end()) {
            return;
        }
        releaseIpSlot(it->second.clientIp);
        (void)registerEvent(kq, fd, EVFILT_READ, EV_DELETE);
        (void)registerEvent(kq, fd, EVFILT_WRITE, EV_DELETE);
        it->second.socket.shutdownReadWrite();
        it->second.socket.close();
        connections.erase(it);
    };

    while (running_.load(std::memory_order_relaxed)) {
        struct kevent events[256];
        timespec timeout{};
        timeout.tv_sec = 1;
        timeout.tv_nsec = 0;

        const int eventCount = ::kevent(kq, nullptr, 0, events, 256, &timeout);
        if (eventCount < 0) {
            if (errno == EINTR) {
                continue;
            }
            logger_.error("kevent wait failed: " + std::string(std::strerror(errno)));
            break;
        }

        for (int i = 0; i < eventCount; ++i) {
            const auto& ev = events[i];
            const int fd = static_cast<int>(ev.ident);

            if ((ev.flags & EV_ERROR) != 0) {
                if (fd != listenSocket_->getFd()) {
                    closeConnection(fd);
                }
                continue;
            }

            if (fd == listenSocket_->getFd() && ev.filter == EVFILT_READ) {
                while (running_.load(std::memory_order_relaxed)) {
                    std::string clientIp;
                    Socket client( -1 );
                    try {
                        client = listenSocket_->accept(&clientIp);
                    } catch (const std::exception&) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;
                        }
                        break;
                    }

                    if (!tryAcquireIpSlot(clientIp)) {
                        http::HttpResponse response = handlers::create429();
                        response.setHeader("Connection", "close");
                        const std::string payload = response.serialize();
                        try {
                            (void)client.send(payload.c_str(), payload.size());
                        } catch (const std::exception&) {
                        }
                        continue;
                    }

                    try {
                        client.setNonBlocking();
                        client.setKeepAlive();
                    } catch (const std::exception&) {
                        releaseIpSlot(clientIp);
                        continue;
                    }

                    const int clientFd = client.getFd();
                    ConnectionState state{std::move(client), clientIp, "", "", false,
                                          std::chrono::steady_clock::now()};
                    state.readBuffer.reserve(kBufferSize);
                    connections.emplace(clientFd, std::move(state));
                    (void)registerEvent(kq, clientFd, EVFILT_READ, EV_ADD | EV_ENABLE);
                }
                continue;
            }

            auto it = connections.find(fd);
            if (it == connections.end()) {
                continue;
            }
            auto& conn = it->second;

            if (ev.filter == EVFILT_READ) {
                bool shouldClose = false;
                while (true) {
                    ssize_t bytesRead = 0;
                    try {
                        bytesRead = conn.socket.recv(ioBuffer.data(), ioBuffer.size());
                    } catch (const std::exception&) {
                        shouldClose = true;
                        break;
                    }
                    if (bytesRead > 0) {
                        conn.readBuffer.append(ioBuffer.data(), static_cast<std::size_t>(bytesRead));
                        conn.lastActive = std::chrono::steady_clock::now();
                        if (conn.readBuffer.size() > kMaxRequestBytes) {
                            http::HttpResponse bad = handlers::create400("Request too large");
                            bad.setHeader("Connection", "close");
                            conn.writeBuffer += bad.serialize();
                            conn.closeAfterWrite = true;
                            break;
                        }
                        continue;
                    }
                    if (bytesRead == 0) {
                        shouldClose = true;
                    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // Non-blocking socket drained.
                    } else if (errno == EINTR) {
                        continue;
                    } else {
                        shouldClose = true;
                    }
                    break;
                }

                while (!conn.readBuffer.empty() && !conn.closeAfterWrite) {
                    http::HttpRequest request;
                    std::size_t consumed = 0;
                    bool parsed = false;
                    try {
                        parsed = parser.parse(conn.readBuffer.data(), conn.readBuffer.size(), request, consumed);
                    } catch (const std::exception& ex) {
                        http::HttpResponse bad = handlers::create400(ex.what());
                        bad.setHeader("Connection", "close");
                        conn.writeBuffer += bad.serialize();
                        conn.closeAfterWrite = true;
                        conn.readBuffer.clear();
                        break;
                    }
                    if (!parsed) {
                        break;
                    }

                    http::HttpResponse response;
                    try {
                        response = fileHandler_.handle(request);
                    } catch (const std::exception& ex) {
                        response = handlers::create500(ex.what());
                    }
                    response.setHeader("Connection", request.isKeepAlive() ? "keep-alive" : "close");
                    conn.writeBuffer += response.serialize();
                    logger_.log(request.method + " " + request.uri + " " + std::to_string(response.statusCode));
                    conn.lastActive = std::chrono::steady_clock::now();

                    conn.readBuffer.erase(0, consumed);
                    if (!request.isKeepAlive()) {
                        conn.closeAfterWrite = true;
                    }
                }

                if (!conn.writeBuffer.empty()) {
                    (void)registerEvent(kq, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE);
                }
                if (shouldClose && conn.writeBuffer.empty()) {
                    closeConnection(fd);
                    continue;
                }
            }

            if (ev.filter == EVFILT_WRITE) {
                while (!conn.writeBuffer.empty()) {
                    ssize_t sent = 0;
                    try {
                        sent = conn.socket.send(conn.writeBuffer.data(), conn.writeBuffer.size());
                    } catch (const std::exception&) {
                        closeConnection(fd);
                        break;
                    }
                    if (sent > 0) {
                        conn.writeBuffer.erase(0, static_cast<std::size_t>(sent));
                        conn.lastActive = std::chrono::steady_clock::now();
                        continue;
                    }
                    if (sent < 0 && errno == EINTR) {
                        continue;
                    }
                    if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    }
                    closeConnection(fd);
                    break;
                }

                if (connections.find(fd) == connections.end()) {
                    continue;
                }
                if (conn.writeBuffer.empty()) {
                    (void)registerEvent(kq, fd, EVFILT_WRITE, EV_DELETE);
                    if (conn.closeAfterWrite) {
                        closeConnection(fd);
                    }
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        std::vector<int> staleFds;
        staleFds.reserve(connections.size());
        for (const auto& [fd, conn] : connections) {
            if (now - conn.lastActive >= kIdleTimeout) {
                staleFds.push_back(fd);
            }
        }
        for (int fd : staleFds) {
            closeConnection(fd);
        }
    }

    for (const auto& [fd, _] : connections) {
        (void)fd;
    }
    std::vector<int> remainingFds;
    remainingFds.reserve(connections.size());
    for (const auto& [fd, _] : connections) {
        remainingFds.push_back(fd);
    }
    for (int fd : remainingFds) {
        closeConnection(fd);
    }

    ::close(kq);
#endif
}

void HttpServer::handleConnection(Socket clientSocket, std::string clientIp) {
    constexpr std::size_t kBufferSize = 8192;
    constexpr std::size_t kMaxRequestBytes = 10 * 1024 * 1024;
    constexpr auto kIdleTimeout = std::chrono::seconds(60);

    if (!tryAcquireIpSlot(clientIp)) {
        http::HttpResponse response = handlers::create429();
        response.setHeader("Connection", "close");
        const std::string payload = response.serialize();
        try {
            (void)clientSocket.send(payload.c_str(), payload.size());
        } catch (const std::exception&) {
        }
        return;
    }
    struct IpSlotGuard {
        HttpServer* server;
        std::string ip;
        ~IpSlotGuard() { server->releaseIpSlot(ip); }
    } ipSlotGuard{this, clientIp};

    try {
        clientSocket.setReceiveTimeoutSeconds(1);
    } catch (const std::exception& ex) {
        logger_.error(std::string("Failed to set receive timeout: ") + ex.what());
    }

    std::vector<char> buffer(kBufferSize);
    std::string requestBuffer;
    requestBuffer.reserve(kBufferSize);
    http::HttpParser parser;
    auto lastActive = std::chrono::steady_clock::now();

    while (running_.load(std::memory_order_relaxed)) {
        bool progressed = false;

        while (!requestBuffer.empty()) {
            http::HttpRequest request;
            std::size_t consumed = 0;
            bool parsed = false;

            try {
                parsed = parser.parse(requestBuffer.data(), requestBuffer.size(), request, consumed);
            } catch (const std::exception& ex) {
                http::HttpResponse bad = handlers::create400(ex.what());
                bad.setHeader("Connection", "close");
                const std::string payload = bad.serialize();
                try {
                    (void)clientSocket.send(payload.c_str(), payload.size());
                } catch (const std::exception&) {
                }
                return;
            }

            if (!parsed) {
                break;
            }
            progressed = true;

            http::HttpResponse response;
            try {
                response = fileHandler_.handle(request);
            } catch (const std::exception& ex) {
                response = handlers::create500(ex.what());
            }
            response.setHeader("Connection", request.isKeepAlive() ? "keep-alive" : "close");

            const std::string responseStr = response.serialize();
            std::size_t totalSent = 0;
            while (totalSent < responseStr.size()) {
                ssize_t sent = 0;
                try {
                    sent = clientSocket.send(responseStr.data() + totalSent, responseStr.size() - totalSent);
                } catch (const std::exception&) {
                    return;
                }
                if (sent < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return;
                }
                if (sent == 0) {
                    return;
                }
                totalSent += static_cast<std::size_t>(sent);
            }

            logger_.log(request.method + " " + request.uri + " " + std::to_string(response.statusCode));
            lastActive = std::chrono::steady_clock::now();

            if (consumed > requestBuffer.size()) {
                return;
            }
            requestBuffer.erase(0, consumed);

            if (!request.isKeepAlive()) {
                return;
            }
        }

        if (requestBuffer.size() > kMaxRequestBytes) {
            http::HttpResponse bad = handlers::create400("Request too large");
            bad.setHeader("Connection", "close");
            const std::string payload = bad.serialize();
            try {
                (void)clientSocket.send(payload.c_str(), payload.size());
            } catch (const std::exception&) {
            }
            return;
        }

        ssize_t bytesRead = 0;
        try {
            bytesRead = clientSocket.recv(buffer.data(), buffer.size());
        } catch (const std::exception&) {
            return;
        }
        if (bytesRead == 0) {
            return;
        }
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (std::chrono::steady_clock::now() - lastActive >= kIdleTimeout) {
                    return;
                }
                continue;
            }
            if (errno == EINTR) {
                continue;
            }
            return;
        }

        requestBuffer.append(buffer.data(), static_cast<std::size_t>(bytesRead));
        lastActive = std::chrono::steady_clock::now();
        if (!progressed && bytesRead == 0) {
            return;
        }
    }
}

bool HttpServer::tryAcquireIpSlot(const std::string& clientIp) {
    if (clientIp.empty()) {
        return true;
    }
    std::lock_guard<std::mutex> lock(ipMutex_);
    std::size_t& count = ipConnections_[clientIp];
    if (count >= maxConnectionsPerIp_) {
        return false;
    }
    ++count;
    return true;
}

void HttpServer::releaseIpSlot(const std::string& clientIp) {
    if (clientIp.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(ipMutex_);
    const auto it = ipConnections_.find(clientIp);
    if (it == ipConnections_.end()) {
        return;
    }
    if (it->second > 1) {
        --it->second;
    } else {
        ipConnections_.erase(it);
    }
}

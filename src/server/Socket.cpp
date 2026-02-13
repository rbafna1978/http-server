#include "server/Socket.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

Socket::Socket() : fd_(::socket(AF_INET, SOCK_STREAM, 0)) {
    if (fd_ < 0) {
        throw makeError("socket() failed");
    }
}

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() {
    closeIfValid();
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        closeIfValid();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

void Socket::bind(int port) const {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        throw makeError("bind() failed");
    }
}

void Socket::listen(int backlog) const {
    if (::listen(fd_, backlog) < 0) {
        throw makeError("listen() failed");
    }
}

Socket Socket::accept(std::string* peerIp) const {
    sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);

    int clientFd;
    do {
        clientFd = ::accept(fd_, reinterpret_cast<sockaddr*>(&clientAddr), &len);
    } while (clientFd < 0 && errno == EINTR);

    if (clientFd < 0) {
        throw makeError("accept() failed");
    }

    if (peerIp != nullptr) {
        char ipBuffer[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuffer, sizeof(ipBuffer)) != nullptr) {
            *peerIp = ipBuffer;
        } else {
            peerIp->clear();
        }
    }

    return Socket(clientFd);
}

ssize_t Socket::send(const char* data, std::size_t len) const {
    ssize_t sent;
    do {
        sent = ::send(fd_, data, len, MSG_NOSIGNAL);
    } while (sent < 0 && errno == EINTR);
    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        throw makeError("send() failed");
    }
    return sent;
}

ssize_t Socket::recv(char* buffer, std::size_t size) const {
    ssize_t bytes;
    do {
        bytes = ::recv(fd_, buffer, size, 0);
    } while (bytes < 0 && errno == EINTR);
    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        throw makeError("recv() failed");
    }
    return bytes;
}

void Socket::setNonBlocking() const {
    const int flags = fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        throw makeError("fcntl(F_GETFL) failed");
    }
    if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw makeError("fcntl(F_SETFL) failed");
    }
}

void Socket::setReuseAddr() const {
    int opt = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw makeError("setsockopt(SO_REUSEADDR) failed");
    }
}

void Socket::setKeepAlive() const {
    int opt = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
        throw makeError("setsockopt(SO_KEEPALIVE) failed");
    }
}

void Socket::setReceiveTimeoutSeconds(int seconds) const {
    timeval tv{};
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        throw makeError("setsockopt(SO_RCVTIMEO) failed");
    }
}

void Socket::shutdownReadWrite() const {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
    }
}

void Socket::close() {
    closeIfValid();
}

std::runtime_error Socket::makeError(const std::string& prefix) {
    return std::runtime_error(prefix + ": errno=" + std::to_string(errno) + " (" + std::strerror(errno) + ")");
}

void Socket::closeIfValid() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

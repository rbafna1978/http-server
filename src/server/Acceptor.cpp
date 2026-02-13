#include "server/Acceptor.h"

#include <cerrno>
#include <memory>

Acceptor::Acceptor(Socket listenSocket, threadpool::ThreadPool& threadPool,
                   std::function<void(Socket, std::string)> connectionHandler)
    : listenSocket_(std::move(listenSocket)),
      threadPool_(threadPool),
      connectionHandler_(std::move(connectionHandler)) {}

Acceptor::~Acceptor() {
    stop();
}

void Acceptor::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }

    acceptThread_ = std::thread([this]() { acceptLoop(); });
}

void Acceptor::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }

    listenSocket_.shutdownReadWrite();
    listenSocket_.close();
    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }
}

void Acceptor::acceptLoop() {
    while (running_.load(std::memory_order_relaxed)) {
        try {
            std::string clientIp;
            Socket client = listenSocket_.accept(&clientIp);
            client.setKeepAlive();
            auto sockPtr = std::make_shared<Socket>(std::move(client));
            threadPool_.submit([this, sockPtr, clientIp]() mutable {
                connectionHandler_(std::move(*sockPtr), clientIp);
            });
        } catch (...) {
            if (!running_.load(std::memory_order_relaxed)) {
                break;
            }
        }
    }
}

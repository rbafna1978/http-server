#pragma once

#include <atomic>
#include <functional>
#include <thread>

#include "server/Socket.h"
#include "threadpool/ThreadPool.h"

class Acceptor {
public:
    Acceptor(Socket listenSocket, threadpool::ThreadPool& threadPool,
             std::function<void(Socket, std::string)> connectionHandler);
    ~Acceptor();

    Acceptor(const Acceptor&) = delete;
    Acceptor& operator=(const Acceptor&) = delete;

    void start();
    void stop();

private:
    void acceptLoop();

    Socket listenSocket_;
    threadpool::ThreadPool& threadPool_;
    std::atomic<bool> running_{false};
    std::function<void(Socket, std::string)> connectionHandler_;
    std::thread acceptThread_;
};

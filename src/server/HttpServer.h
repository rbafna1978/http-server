#pragma once

#include <atomic>
#include <mutex>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "handlers/FileHandler.h"
#include "server/Acceptor.h"
#include "server/Socket.h"
#include "threadpool/ThreadPool.h"
#include "utils/FileCache.h"
#include "utils/Logger.h"

class HttpServer {
public:
    HttpServer(int port, std::size_t numThreads, std::string docRoot, bool useKqueue = false);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void start();
    void stop();

private:
    void runKqueueLoop();
    void handleConnection(Socket clientSocket, std::string clientIp);
    bool tryAcquireIpSlot(const std::string& clientIp);
    void releaseIpSlot(const std::string& clientIp);

    int port_;
    std::string docRoot_;
    bool useKqueue_;

    threadpool::ThreadPool threadPool_;
    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<Socket> listenSocket_;
    std::thread ioThread_;
    FileCache fileCache_;
    FileHandler fileHandler_;
    Logger logger_;
    std::atomic<bool> running_{false};
    std::mutex ipMutex_;
    std::unordered_map<std::string, std::size_t> ipConnections_;
    const std::size_t maxConnectionsPerIp_{100};
};

#include "server/HttpServer.h"

#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

namespace {
std::unique_ptr<HttpServer> g_server;
volatile std::sig_atomic_t g_stopRequested = 0;

void signalHandler(int) {
    g_stopRequested = 1;
}
}  // namespace

int main(int argc, char* argv[]) {
    int port = 8080;
    std::size_t numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) {
        numThreads = 4;
    }
    std::string docRoot = "./public";
    bool useKqueue = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--threads" && i + 1 < argc) {
            numThreads = static_cast<std::size_t>(std::stoull(argv[++i]));
        } else if (arg == "--root" && i + 1 < argc) {
            docRoot = argv[++i];
        } else if (arg == "--kqueue") {
            useKqueue = true;
        }
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        g_server = std::make_unique<HttpServer>(port, numThreads, docRoot, useKqueue);

        std::cout << "Starting HTTP server on port " << port << "\n";
        std::cout << "Document root: " << docRoot << "\n";
        std::cout << "Thread pool size: " << numThreads << "\n";
        std::cout << "Mode: " << (useKqueue ? "kqueue" : "thread-pool") << "\n";

        g_server->start();

        while (!g_stopRequested) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        if (g_server) {
            g_server->stop();
        }
    } catch (const std::exception& ex) {
        std::cerr << "Server error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

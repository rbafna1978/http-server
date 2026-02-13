#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "threadpool/Task.h"
#include "threadpool/WorkStealingQueue.h"

namespace threadpool {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t numThreads = std::thread::hardware_concurrency());
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submit(Task task);
    void shutdown();

private:
    std::optional<Task> tryGetTask(std::size_t threadId);
    void workerThread(std::size_t threadId);

    std::vector<std::thread> workers_;
    std::vector<std::unique_ptr<WorkStealingQueue>> queues_;
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> nextQueue_{0};
    std::atomic<std::size_t> pendingTasks_{0};

    std::mutex cvMutex_;
    std::condition_variable cv_;
};

}  // namespace threadpool

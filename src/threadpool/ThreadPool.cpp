#include "threadpool/ThreadPool.h"

#include <stdexcept>

namespace threadpool {

ThreadPool::ThreadPool(std::size_t numThreads) {
    if (numThreads == 0) {
        numThreads = 1;
    }

    queues_.reserve(numThreads);
    for (std::size_t i = 0; i < numThreads; ++i) {
        queues_.push_back(std::make_unique<WorkStealingQueue>());
    }

    workers_.reserve(numThreads);
    for (std::size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this, i]() { workerThread(i); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::submit(Task task) {
    if (stop_.load(std::memory_order_relaxed)) {
        throw std::runtime_error("Cannot submit task to stopped ThreadPool");
    }
    const std::size_t index = nextQueue_.fetch_add(1, std::memory_order_relaxed) % queues_.size();
    queues_[index]->push(std::move(task));
    pendingTasks_.fetch_add(1, std::memory_order_release);
    cv_.notify_one();
}

void ThreadPool::shutdown() {
    bool expected = false;
    if (!stop_.compare_exchange_strong(expected, true)) {
        return;
    }

    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::workerThread(std::size_t threadId) {
    while (true) {
        auto task = tryGetTask(threadId);
        if (task) {
            try {
                (*task)();
            } catch (...) {
                // Worker keeps processing subsequent tasks.
            }
            continue;
        }

        std::unique_lock<std::mutex> lock(cvMutex_);
        cv_.wait(lock, [this]() {
            return stop_.load(std::memory_order_relaxed) ||
                   pendingTasks_.load(std::memory_order_acquire) > 0;
        });

        if (stop_.load(std::memory_order_relaxed) &&
            pendingTasks_.load(std::memory_order_acquire) == 0) {
            break;
        }
    }
}

std::optional<Task> ThreadPool::tryGetTask(std::size_t threadId) {
    auto task = queues_[threadId]->pop();
    if (!task) {
        for (std::size_t i = 0; i < queues_.size(); ++i) {
            if (i == threadId) {
                continue;
            }
            task = queues_[i]->steal();
            if (task) {
                break;
            }
        }
    }

    if (task) {
        (void)pendingTasks_.fetch_sub(1, std::memory_order_acq_rel);
    }
    return task;
}

}  // namespace threadpool

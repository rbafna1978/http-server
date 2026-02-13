#include "threadpool/WorkStealingQueue.h"

namespace threadpool {

void WorkStealingQueue::push(Task task) {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push_back(std::move(task));
}

std::optional<Task> WorkStealingQueue::pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tasks_.empty()) {
        return std::nullopt;
    }
    Task task = std::move(tasks_.back());
    tasks_.pop_back();
    return task;
}

std::optional<Task> WorkStealingQueue::steal() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tasks_.empty()) {
        return std::nullopt;
    }
    Task task = std::move(tasks_.front());
    tasks_.pop_front();
    return task;
}

std::size_t WorkStealingQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

}  // namespace threadpool

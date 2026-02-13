#pragma once

#include <deque>
#include <mutex>
#include <optional>

#include "threadpool/Task.h"

namespace threadpool {

class WorkStealingQueue {
public:
    void push(Task task);
    std::optional<Task> pop();
    std::optional<Task> steal();
    std::size_t size() const;

private:
    mutable std::mutex mutex_;
    std::deque<Task> tasks_;
};

}  // namespace threadpool

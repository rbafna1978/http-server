#pragma once

#include <functional>

namespace threadpool {
using Task = std::function<void()>;
}  // namespace threadpool

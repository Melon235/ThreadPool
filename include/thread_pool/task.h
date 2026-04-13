#pragma once
#include <functional>

namespace thread_pool {

using Task = std::function<void()>;

} // namespace thread_pool
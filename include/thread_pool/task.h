#pragma once
#include <functional>

namespace thread_pool {


/**
* @brief 线程池统一任务形式
*
* 所有函数最终都会在submit()被包装成task进入线程池内部
*/
using Task = std::function<void()>;

/**
* @brief 策略调度选项
*   FIFO 无此参数
*   PriorityPolicy 使用 priority
*   WorkStealingPolicy 使用 hint_worker
*   @param hint_worker 提示调度器“这个任务更倾向于交给哪个 worker”
*   @param priority 该任务优先级
*/
struct ScheduleOptions {
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    std::size_t hint_worker = npos;
    int priority = 0;
};

} // namespace thread_pool
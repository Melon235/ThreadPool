#pragma once

#include <cstddef>
#include "thread_pool/task.h"

namespace thread_pool {

/**
 * @brief 调度策略接口
 *
 * 负责定义任务的入队、出队和关闭行为
 * 不同调度策略（FIFO / Priority / WorkStealing）通过该接口实现
 * 向上层 Scheduler 提供统一的调度能力
 */
class ISchedulePolicy {
public:
    /// @brief 无效 worker 标识，用于不指定目标 worker 的场景
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    virtual ~ISchedulePolicy() = default;

    /**
     * @brief 任务入队
     * @param task 要加入调度队列的任务
     * @param hint_worker 目标 worker 提示，FIFO 策略可忽略，
     *        为后续 WorkStealing 策略预留
     */
    virtual void enqueue(Task task, std::size_t hint_worker = npos) = 0;

    /**
     * @brief 任务出队
     * @param worker_id 当前任务的 worker 编号
     * @return 成功取出的任务；若调度器已关闭且无任务可取，返回空任务
     */
    virtual Task dequeue(std::size_t worker_id) = 0;

    /**
     * @brief 关闭调度策略
     *
     * 用于通知策略层停止等待、唤醒阻塞线程，并为线程池关闭做准备。
     */
    virtual void shutdown() = 0;
};

} // namespace thread_pool
#pragma once

#include <cstddef>
#include "thread_pool/task.h"


namespace thread_pool {

/**
 * @brief 使用策略
 */
enum class PolicyType {
    FIFO,
    PRIORITY,
    WORKSTEALING
};

/**
 * @brief 当前线程所属的 worker id
 *
 * 用于区分当前提交任务的线程是否为线程池内部 worker。
 *
 * 约定：
 * - 当值为 -1 时，表示当前线程不是 worker 线程
 * - 当值 >= 0 时，表示当前线程是对应编号的 worker 线程
 *
 * WorkStealingPolicy 会根据该值决定任务进入：
 * - worker 本地队列
 * - 全局注入队列
 */
extern thread_local int tls_worker_id;

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
    virtual void enqueue(Task task, const ScheduleOptions& opts = {}) = 0;

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
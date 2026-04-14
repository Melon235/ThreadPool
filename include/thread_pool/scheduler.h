#pragma once

#include <cstddef>
#include <memory>

#include "thread_pool/policy.h"
#include "thread_pool/task.h"

namespace thread_pool {

/**
 * @brief 调度器
 *
 * 作为线程池与具体调度策略之间的中间层，
 * 向上提供统一的任务提交与获取接口，
 * 向下委托具体策略完成调度行为。
 */
class Scheduler {
public:
    /**
     * @brief 构造调度器
     * @param policy 调度策略对象
     */
    explicit Scheduler(std::unique_ptr<ISchedulePolicy> policy);

    ~Scheduler() = default;

    /**
     * @brief 提交任务到调度器
     * @param task 要调度的任务
     * @param hint_worker 目标 worker 提示，供特定策略使用
     */
    void schedule(Task task, const ScheduleOptions& opts = {});

    /**
     * @brief 获取任务
     * @param worker_id 当前请求任务的 worker 编号
     * @return 取出的任务；若返回空任务，说明无任务可执行
     */
    Task fetch_task(std::size_t worker_id);

    /**
     * @brief 关闭调度器
     *
     * 通知底层策略停止等待并准备退出。
     */
    void shutdown();

private:
    std::unique_ptr<ISchedulePolicy> policy_;
};

} // namespace thread_pool
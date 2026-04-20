#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <vector>

#include "thread_pool/policy.h"

namespace thread_pool {




/**
 * @brief Work Stealing 调度策略
 *
 * 该策略采用“每个 worker 一个本地双端队列 + 一个全局注入队列”的结构。
 *
 * 调度规则如下：
 * - 外部线程提交的任务进入全局队列
 * - worker 线程内部提交的任务进入当前 worker 的本地队列
 * - worker 获取任务时，优先从自己的本地队列尾部取任务
 * - 若本地队列为空，则尝试从全局队列获取任务
 * - 若全局队列也为空，则尝试从其他 worker 的本地队列头部窃取任务
 
 */
class WorkStealingPolicy : public ISchedulePolicy {
public:

    explicit WorkStealingPolicy(std::size_t worker_count);
    ~WorkStealingPolicy() override = default;


    void enqueue(Task task, const ScheduleOptions& opts) override;
    Task dequeue(std::size_t worker_id) override;
    void shutdown() override;

    /**
     * @brief 判断当前是否没有待执行任务
     *
     * 当且仅当满足以下条件时返回 true：
     * - 全局队列为空
     * - 所有 worker 的本地队列均为空
     */
    bool empty() const;

private:
    /**
     * @brief worker 的本地任务队列
     *
     * 每个 worker 拥有一个独立的本地双端队列。
     * 访问规则：
     * - owner worker 从队尾取任务（pop_back）
     * - 其他 worker 从队首窃取任务（pop_front）
     */
    struct LocalQueue {
        std::deque<Task> tasks;   
        mutable std::mutex mtx;   
    };

private:
    /**
     * @brief 尝试从当前 worker 的本地队列尾部取任务
     */
    bool try_pop_local(std::size_t worker_id, Task& out);

    /**
     * @brief 尝试从全局队列头部取任务
     */
    bool try_pop_global(Task& out);

    /**
     * @brief 尝试从其他 worker 的本地队列窃取任务
     *
     * 当前 worker 会按既定顺序遍历其他 worker，
     * 若发现某个 victim 的本地队列非空，则从其队首窃取一个任务。
     * @param thief_id 当前尝试窃取任务的 worker 编号
     * @param out 成功时输出窃取到的任务
     */
    bool try_steal(std::size_t thief_id, Task& out);




private:
    std::size_t worker_count_;                 ///< worker 数量
    std::vector<LocalQueue> local_queues_;     ///< 每个 worker 的本地任务队列

    std::deque<Task> global_queue_;            ///< 外部线程提交任务使用的全局注入队列
    mutable std::mutex global_mtx_;            ///< 保护全局队列的互斥锁

    mutable std::mutex wait_mtx_;              ///< 条件变量等待使用的互斥锁
    std::condition_variable cv_;               ///< 用于任务到来或 shutdown 时唤醒 worker

    std::atomic<bool> stopping_{false};        ///< 停止标志，true 表示不再接受新任务

    std::atomic<std::uint64_t> pending_task_count_{0}; ///< 确定全局状态，查找是否有任务

    

};

}  // namespace thread_pool
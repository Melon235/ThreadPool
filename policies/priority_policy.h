#pragma once

#include <cstdint>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

#include "thread_pool/policy.h"
#include "thread_pool/task.h"

namespace thread_pool {

/**
 * @brief Priority 调度策略
 *
 * 按任务进入队列的priority参数进行调度。
 * 该策略内部维护一个共享任务队列，
 * 该队列存储调度节点
 * 所有 worker 都从该队列中获取任务。
 */
class PriorityPolicy : public ISchedulePolicy {
    public:
    PriorityPolicy() = default;
    ~PriorityPolicy() override = default;

    void enqueue(Task task, const ScheduleOptions& opts = {}) override;
    Task dequeue(std::size_t worker_id) override;
    void shutdown() override;

    private:

    /**
    * @brief 内部待调度节点
    *
    *  - task: 真正执行体
    *  - priority: 调度优先级，数值越大优先级越高
    *  - seq: 同优先级下用于保证 FIFO 的提交序号，越小表示越早入队
    */
    struct Node {
        Task task;
        int priority{0};
        std::uint64_t seq{0};
    };

    /**
    * @brief Priority_queue 比较规则
    *
    *  1. priority 大者优先
    *  2. 若 priority 相同，则 seq 小者优先
    *  3. 使 top() 始终是“当前最该先执行”的任务
    */
    struct Compare {
        bool operator()(const Node& lhs, const Node& rhs) const;
    };

    private:
    // 待调度任务集合
    std::priority_queue<Node, std::vector<Node>, Compare> queue_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    // 关闭标志：
    // true  表示不再接收新任务，但允许取完已入队任务
    // false 表示正常运行
    bool stopping_{false};

    // 同优先级 FIFO 的顺序号分配器
    std::uint64_t next_seq_{0};
};

}  // namespace thread_pool
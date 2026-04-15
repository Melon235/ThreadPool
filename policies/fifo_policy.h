#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

#include "thread_pool/policy.h"
#include "thread_pool/task.h"

namespace thread_pool {

/**
 * @brief FIFO 调度策略
 *
 * 按任务进入队列的先后顺序进行调度。
 * 该策略内部维护一个共享任务队列，
 * 所有 worker 都从该队列中获取任务。
 */
class FifoPolicy : public ISchedulePolicy {
public:
    FifoPolicy() = default;
    ~FifoPolicy() override = default;

    void enqueue(Task task, const ScheduleOptions& opts = {}) override;
    Task dequeue(std::size_t worker_id) override;
    void shutdown() override;

private:
    std::queue<Task> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    // 关闭标志：
    // true  表示不再接收新任务，但允许取完已入队任务
    // false 表示正常运行
    bool stopping_ {false};
};

} // namespace thread_pool
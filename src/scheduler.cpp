#include "thread_pool/scheduler.h"

#include <stdexcept>
#include <utility>

namespace thread_pool {

Scheduler::Scheduler(std::unique_ptr<ISchedulePolicy> policy)
    // TODO: 初始化 policy_
{
    // TODO:
    // 1. 检查传入的 policy 是否为空
    // 2. 若为空，抛异常
}

void Scheduler::schedule(Task task, std::size_t hint_worker) {
    // TODO:
    // 1. 可选：检查空任务
    // 2. 调用 policy_->enqueue(...)
}

Task Scheduler::fetch_task(std::size_t worker_id) {
    // TODO:
    // 1. 直接返回 policy_->dequeue(worker_id)
}

void Scheduler::shutdown() {
    // TODO:
    // 1. 调用 policy_->shutdown()
}

} // namespace thread_pool
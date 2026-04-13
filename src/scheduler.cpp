#include "thread_pool/scheduler.h"

#include <stdexcept>
#include <utility>

namespace thread_pool {

Scheduler::Scheduler(std::unique_ptr<ISchedulePolicy> policy)
{
    if(!policy) throw std::invalid_argument("policy cannot be empty!");
    policy_ = std::move(policy);
}

void Scheduler::schedule(Task task, std::size_t hint_worker) {
    if(!task) throw std::invalid_argument("task is empty!");
    policy_->enqueue(std::move(task), hint_worker);
}

Task Scheduler::fetch_task(std::size_t worker_id) {
    return policy_->dequeue(worker_id);
}

void Scheduler::shutdown() {
    policy_->shutdown();
}

} // namespace thread_pool
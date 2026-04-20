#include "thread_pool/scheduler.h"


#include <stdexcept>
#include <utility>

#include "fifo_policy.h"
#include "priority_policy.h"
#include "work_stealing_policy.h"

namespace thread_pool {

Scheduler::Scheduler(PolicyType type, std::size_t worker_count) {
    switch (type) {
        case PolicyType::FIFO:
            policy_ = std::make_unique<FifoPolicy>();
            break;

        case PolicyType::PRIORITY:
            policy_ = std::make_unique<PriorityPolicy>();
            break;

        case PolicyType::WORKSTEALING:
            policy_ = std::make_unique<WorkStealingPolicy>(worker_count);
            break;

        default:
            throw std::invalid_argument("unknown policy type");
    }
}

void Scheduler::schedule(Task task, const ScheduleOptions& opts) {
    if(!task) throw std::invalid_argument("task is empty!");
    policy_->enqueue(std::move(task), opts);
}

Task Scheduler::fetch_task(std::size_t worker_id) {
    return policy_->dequeue(worker_id);
}

void Scheduler::shutdown() {
    policy_->shutdown();
}

} // namespace thread_pool
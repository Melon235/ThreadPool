#include "priority_policy.h"

#include <stdexcept>
#include <utility>

namespace thread_pool {

bool PriorityPolicy::Compare::operator()(const Node& lhs, const Node& rhs) const {
    if(lhs.priority != rhs.priority){
        return lhs.priority < rhs.priority;
    }
    return lhs.seq > rhs.seq; 
    return false;
}

void PriorityPolicy::enqueue(Task task, const ScheduleOptions& opts) {
    if(!task)
        throw std::invalid_argument("task must not be empty");
    // 上锁
    {
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_){
        throw std::logic_error("scheduler is stopping");
    }
    auto seq = next_seq_++;
    Node priority_task{std::move(task), opts.priority, seq};
    queue_.push(std::move(priority_task));
    }
    cv_.notify_one();
}

Task PriorityPolicy::dequeue(std::size_t worker_id) {
    (void)worker_id;
    std::unique_lock<std::mutex> lck(mutex_);
    // 等待条件: queue_非空，stopping_ -> true
    cv_.wait(lck, [this]{return !queue_.empty() || stopping_;});

    // case 1 queue_非空 -> 出队
    if(!queue_.empty()){
        Task cur_task = queue_.top().task;
        queue_.pop();
        return cur_task;
    }

    // case 2 queue_空 && 线程池关闭stopping_ -> 通知worker shutdown
    return Task{};
}

void PriorityPolicy::shutdown() {
     {
        std::unique_lock<std::mutex> lck(mutex_);
        stopping_ = true; 
    }
    cv_.notify_all();
}

}  // namespace thread_pool
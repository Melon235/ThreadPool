#include "work_stealing_policy.h"

#include <stdexcept>
#include <utility>
#include <iostream>

namespace thread_pool {

WorkStealingPolicy::WorkStealingPolicy(std::size_t worker_count)
    : worker_count_(worker_count),
      local_queues_(worker_count) {
    // TODO:
    // 1. 检查 worker_count 是否为 0
    // 2. 若为 0，抛出 std::invalid_argument
    // 3. 确认 local_queues_ 的大小与 worker_count_ 一致
    if(!worker_count)
        throw  std::invalid_argument("worker_count must be greater than 0");
    if(worker_count_ != local_queues_.size())
        throw std::logic_error("local_queues_.size() does not match worker_count_");
}

void WorkStealingPolicy::enqueue(Task task, const ScheduleOptions& opts) {
    
    (void)opts;

    if(!task)
        throw std::invalid_argument("task must not be empty");

    if(stopping_)
        throw std::logic_error("scheduler is stopping");

    // case 1 worker线程 -> local_queue
    if(tls_worker_id >= 0 && tls_worker_id < worker_count_){
        auto& local_queue = local_queues_[tls_worker_id];
        {
            std::lock_guard<std::mutex> lock(local_queue.mtx);
            local_queue.tasks.push_back(std::move(task));
            pending_task_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // case 2 外部线程 -> global_queue
    else{
        {
            std::lock_guard<std::mutex> lock(global_mtx_);
            global_queue_.push_back(std::move(task));
            pending_task_count_.fetch_add(1, std::memory_order_relaxed);
            
        }
    }
    cv_.notify_one();
}

Task WorkStealingPolicy::dequeue(std::size_t worker_id) {
    Task task;
    //  每轮按固定顺序尝试：
    //    1 try_pop_local(worker_id, task)
    //    2 try_pop_global(task)
    //    3 try_steal(worker_id, task)
    while (true) {
        if (try_pop_local(worker_id, task)) {
            return task;
        }

        if (try_pop_global(task)) {
            return task;
        }

        if (try_steal(worker_id, task)) {
            return task;
        }

        std::unique_lock<std::mutex> lock(wait_mtx_);
        cv_.wait(lock, [this]() {
            return stopping_.load() || pending_task_count_.load(std::memory_order_relaxed) > 0;
        });

        if (stopping_ && pending_task_count_.load(std::memory_order_relaxed) == 0) {
            return Task{};
        }
    }
    return Task{};
}

void WorkStealingPolicy::shutdown() {
    stopping_.store(true);
    cv_.notify_all();
}

bool WorkStealingPolicy::empty() const {
    return pending_task_count_.load(std::memory_order_relaxed);
}

bool WorkStealingPolicy::try_pop_local(std::size_t worker_id, Task& out) {

    if (worker_id >= worker_count_) {
        return false;
    }

    auto& local_queue = local_queues_[worker_id];
    std::lock_guard<std::mutex> lock(local_queue.mtx);

    if (local_queue.tasks.empty()) {
        return false;
    }

    out = std::move(local_queue.tasks.back());
    local_queue.tasks.pop_back();
    pending_task_count_.fetch_sub(1, std::memory_order_relaxed); // atomic--

    return true;
}

bool WorkStealingPolicy::try_pop_global(Task& out) {
    std::lock_guard<std::mutex> lock(global_mtx_);

    if (global_queue_.empty()) {
        return false;
    }

    out = std::move(global_queue_.front());
    global_queue_.pop_front();
    pending_task_count_.fetch_sub(1, std::memory_order_relaxed); // atomic--

    return true;
}

bool WorkStealingPolicy::try_steal(std::size_t thief_id, Task& out) {

    if (worker_count_ <= 1) {
        return false;
    }

    // stealing
    for (std::size_t step = 1; step < worker_count_; ++step) {
        
        // 选取victim
        std::size_t victim_id = (thief_id + step) % worker_count_;
        auto& victim_queue = local_queues_[victim_id];

        std::lock_guard<std::mutex> lock(victim_queue.mtx);
        if (victim_queue.tasks.empty()) {
            continue;
        }

        out = std::move(victim_queue.tasks.front());
        victim_queue.tasks.pop_front();
        pending_task_count_.fetch_sub(1, std::memory_order_relaxed); // atomic--

        return true;
    }

    return false;
}


}  // namespace thread_pool
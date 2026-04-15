#include "fifo_policy.h"

#include <utility>
#include <iostream>
#include <atomic>

namespace thread_pool {

// task入队
void FifoPolicy::enqueue(Task task, const ScheduleOptions& opts) {
    
    (void)opts; // FIFO中无该参数
    // 空任务保护
    if(!task){
        return;
    }
    // queue_上锁
    {
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_){
        return;
    }
    queue_.push(std::move(task));
    }

    cv_.notify_one();
}

Task FifoPolicy::dequeue(std::size_t worker_id) {

    (void)worker_id; // FIFO无参数
    std::unique_lock<std::mutex> lck(mutex_);
    // 等待条件: queue_非空，waiting_ -> true
    cv_.wait(lck, [this]{return !queue_.empty() || stopping_;});

    // case 1 queue_非空 -> 出队
    if(!queue_.empty()){
        Task cur_task = std::move(queue_.front());
        queue_.pop();
        return cur_task;
    }

    // case 2 queue_空 && 线程池关闭stopping_ -> 通知worker shutdown
    return Task{};
    
}

void FifoPolicy::shutdown() {
    // TODO:
    // 1. 加锁
    // 2. 将 stopping_ 置为 true
    //    - 注意重复调用时要保持幂等
    // 3. 解锁
    // 4. notify_all，唤醒所有可能阻塞在 dequeue() 的 worker
    {
        std::unique_lock<std::mutex> lck(mutex_);
        stopping_ = true; 
    }
    cv_.notify_all();
}

} // namespace thread_pool
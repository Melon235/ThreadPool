#include "thread_pool/thread_pool.h"

#include <stdexcept>
#include <utility>
#include <iostream>


namespace thread_pool {

ThreadPool::ThreadPool(std::size_t thread_count, PolicyType type):
thread_count_(thread_count), policy_type_(type){
    
    if(thread_count == 0){
        throw std::invalid_argument("thread_count must be greater than 0!");
    }

    scheduler_ = std::make_unique<Scheduler>(policy_type_, thread_count_);
    
    // 创建workers_
    workers_.reserve(thread_count_);
    for(int i = 0; i < thread_count_; i++){
        workers_.push_back(std::make_unique<Worker>(i, scheduler_.get()));
    }
    state_ = PoolState::Created;
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::start() {
    if(state_ != PoolState::Created){
        throw std::logic_error("threadpool can only be started from Created state!");
    }
    
    for (auto& worker : workers_) {
        worker->start();
    }
    state_ = PoolState::Running;

}

// submit()
// 默认任务提交入口：使用默认调度参数。
// 对 FIFO 普通入队；
// 对 PriorityPolicy 等价于默认优先级；
// 对 WorkStealingPolicy 表示未指定额外调度提示
void ThreadPool::submit(Task task) {
    submit(std::move(task), ScheduleOptions{});
}

// submit() opts重载

void ThreadPool::submit(Task task, const ScheduleOptions& opts){
    if(!task) throw std::invalid_argument("task should not be empty!");
    if(state_ != PoolState::Running){
        throw std::logic_error("threadpool can only be submitted from Running state!");
    }    
    scheduler_->schedule(std::move(task), opts);
    
}

void ThreadPool::shutdown() {
    if(state_ == PoolState::Stopped) return;

    scheduler_->shutdown();

    for(std::size_t i = 0; i < workers_.size(); ++i){
        workers_[i]->join();
    }
    state_ = PoolState::Stopped;
}

std::size_t ThreadPool::thread_count() const noexcept {
    return thread_count_;
}

PoolState ThreadPool::state() const noexcept {
    return state_;
}

} // namespace thread_pool
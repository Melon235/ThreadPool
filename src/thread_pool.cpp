#include "thread_pool/thread_pool.h"

#include <stdexcept>
#include <utility>

#include "fifo_policy.h"

namespace thread_pool {

ThreadPool::ThreadPool(std::size_t thread_count):
thread_count_(thread_count)
{
    if(thread_count == 0){
        throw std::invalid_argument("thread_count must be greater than 0!");
    }
    auto policy = std::make_unique<FifoPolicy>();
    scheduler_ = std::make_unique<Scheduler>(std::move(policy));
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

void ThreadPool::submit(Task task) {
    if(!task) throw std::invalid_argument("task should not be empty!");
    if(state_ != PoolState::Running){
        throw std::logic_error("threadpool can only be submitted from Running state!");
    }    
    scheduler_->schedule(std::move(task));
    
}

void ThreadPool::shutdown() {
    if(state_ == PoolState::Stopped) return;
    scheduler_->shutdown();
    for(auto& worker:workers_){
        worker->join();
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
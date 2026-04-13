#include "thread_pool/worker.h"

#include <stdexcept>
#include <utility>
#include <iostream>

namespace thread_pool {

Worker::Worker(std::size_t id, Scheduler* scheduler)
    :id_(id), scheduler_(scheduler)
{
    if(!scheduler) 
        throw std::invalid_argument("scheduler is empty!");
}

void Worker::start() {
    if(thread_.joinable()){
        throw std::runtime_error("worker already started");
    }
    // 启动线程
    thread_ = std::thread(&Worker::loop, this);
}

void Worker::loop() {
    while(true){
        Task cur_task;
        // 获取任务
        cur_task = scheduler_->fetch_task(id_);
         // 空任务表示调度器已关闭且无剩余任务，worker 退出
        if(!cur_task){
            break;
        }
        // 执行任务
        run_task(std::move(cur_task));
    }
}

void Worker::run_task(Task task) {
    // 空任务保护
    if(!task) return;
    // 异常保护
    try{
        task();
    }
    catch(...){
        // TODO: 日志记录异常时间 事件
    }

}

void Worker::join() {
    if(thread_.joinable()) 
        thread_.join();
}

} // namespace thread_pool
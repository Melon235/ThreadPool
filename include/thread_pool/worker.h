#pragma once

#include <cstddef>
#include <thread>

#include "thread_pool/scheduler.h"
#include "thread_pool/task.h"

namespace thread_pool {

/**
 * @brief 工作线程对象
 *
 * 封装线程池中的单个工作线程。
 * Worker 启动后会持续从 Scheduler 获取任务并执行，
 * 直到调度器关闭且无任务可取时退出。
 */
class Worker {
public:
    /**
     * @brief 构造工作线程
     * @param id worker 编号
     * @param scheduler 任务调度器
     */
    Worker(std::size_t id, Scheduler* scheduler);

    ~Worker() = default;

    /**
     * @brief 启动工作线程
     */
    void start();

    /**
     * @brief 等待工作线程结束
     */
    void join();

private:
    /**
     * @brief 工作线程主循环
     */
    void loop();

    /**
     * @brief 执行单个任务
     * @param task 要执行的任务
     */
    void run_task(Task task);

private:
    std::size_t id_;
    Scheduler* scheduler_;
    std::thread thread_;
};

} // namespace thread_pool
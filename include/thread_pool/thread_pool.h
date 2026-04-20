#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include <future>

#include "thread_pool/scheduler.h"
#include "thread_pool/task.h"
#include "thread_pool/worker.h"

namespace thread_pool {

/**
 * @brief 线程池状态
 */
enum class PoolState {
    Created,  ///< 已创建，尚未启动
    Running,  ///< 运行中
    Stopped   ///< 已停止
};



/**
 * @brief FIFO 线程池
 *
 * 负责管理线程池生命周期、工作线程集合以及任务提交入口。
 */
class ThreadPool {
public:
    /**
     * @brief 构造线程池
     * @param thread_count 工作线程数量
     */
    explicit ThreadPool(std::size_t thread_count, PolicyType type = PolicyType::FIFO);

    ~ThreadPool();

    /**
     * @brief 启动线程池
     */
    void start();

    /**
     * @brief 提交任务
     * @param task 要提交的任务
     */
    // submit()
    void submit(Task task);
    // priority_policy 重载
    void submit(Task task, const ScheduleOptions& opts);

    // 模板重载
    template <class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto bound_task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        auto packaged = 
            std::make_shared<std::packaged_task<ReturnType()>>(std::move(bound_task));

        std::future<ReturnType> fut = packaged->get_future();

        Task wrapper = [packaged]() {
            (*packaged)();
        };

        submit(std::move(wrapper));
        return fut;
    }
    
    // Priority模板重载
    template <class F, class... Args>
    auto submit(const ScheduleOptions& opts, F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto bound_task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);

        auto packaged =
            std::make_shared<std::packaged_task<ReturnType()>>(std::move(bound_task));

        std::future<ReturnType> fut = packaged->get_future();

        // 显式调用 submit(Task, opts)
        this->submit(Task{[packaged]() {
            (*packaged)();
        }}, opts);

        return fut;
    }
    



    /**
     * @brief 关闭线程池
     *
     * 通知调度器停止等待，并等待所有工作线程退出。
     */
    void shutdown();

    /**
     * @brief 获取线程数量
     */
    std::size_t thread_count() const noexcept;

    /**
     * @brief 获取线程池当前状态
     */
    PoolState state() const noexcept;

private:
    std::unique_ptr<Scheduler> scheduler_;
    std::vector<std::unique_ptr<Worker>> workers_;
    std::size_t thread_count_;
    PoolState state_;
    PolicyType policy_type_;
};

} // namespace thread_pool
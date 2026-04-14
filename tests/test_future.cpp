#include "thread_pool/thread_pool.h"

#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

using thread_pool::PolicyType;
using thread_pool::ScheduleOptions;
using thread_pool::Task;
using thread_pool::ThreadPool;

// 普通函数：用于测试模板 submit(add, 1, 2)
int add(int a, int b) {
    return a + b;
}

// 测试 1：基础 Task 提交（默认 opts）
void test_submit_task_default() {
    ThreadPool pool(2, PolicyType::FIFO);
    pool.start();

    std::atomic<int> counter{0};

    Task task = [&counter]() {
        counter.fetch_add(1);
    };

    pool.submit(std::move(task));
    pool.shutdown();

    assert(counter.load() == 1);
    std::cout << "test_submit_task_default passed\n";
}

// 测试 2：基础 Task + ScheduleOptions 提交
void test_submit_task_with_opts() {
    ThreadPool pool(2, PolicyType::FIFO);
    pool.start();

    std::atomic<int> counter{0};

    Task task = [&counter]() {
        counter.fetch_add(1);
    };

    ScheduleOptions opts;
    opts.priority = 10;  // FIFO 会忽略，但接口必须能走通

    pool.submit(std::move(task), opts);
    pool.shutdown();

    assert(counter.load() == 1);
    std::cout << "test_submit_task_with_opts passed\n";
}

// 测试 3：模板 submit(F, Args...)
void test_submit_template_default() {
    ThreadPool pool(2, PolicyType::FIFO);
    pool.start();

    auto fut = pool.submit(add, 1, 2);
    assert(fut.get() == 3);

    pool.shutdown();
    std::cout << "test_submit_template_default passed\n";
}

// 测试 4：模板 submit(opts, F, Args...)
void test_submit_template_with_opts() {
    ThreadPool pool(2, PolicyType::FIFO);
    pool.start();

    ScheduleOptions opts;
    opts.priority = 20;  // 当前 FIFO 忽略，但必须能传到统一入口

    auto fut = pool.submit(opts, add, 3, 4);
    assert(fut.get() == 7);

    pool.shutdown();
    std::cout << "test_submit_template_with_opts passed\n";
}

// 测试 5：模板 submit(opts, lambda)
void test_submit_lambda_with_opts() {
    ThreadPool pool(2, PolicyType::FIFO);
    pool.start();

    ScheduleOptions opts;
    opts.priority = 5;

    auto fut = pool.submit(opts, []() {
        return 42;
    });

    assert(fut.get() == 42);

    pool.shutdown();
    std::cout << "test_submit_lambda_with_opts passed\n";
}

// 测试 6：模板 submit(opts, void lambda)
void test_submit_void_lambda_with_opts() {
    ThreadPool pool(2, PolicyType::FIFO);
    pool.start();

    std::atomic<int> counter{0};

    ScheduleOptions opts;
    opts.priority = 8;

    auto fut = pool.submit(opts, [&counter]() {
        counter.fetch_add(1);
    });

    fut.get();
    assert(counter.load() == 1);

    pool.shutdown();
    std::cout << "test_submit_void_lambda_with_opts passed\n";
}

// 测试 7：模板 submit(opts, ...) 异常传播
void test_submit_exception_with_opts() {
    ThreadPool pool(2, PolicyType::FIFO);
    pool.start();

    ScheduleOptions opts;
    opts.priority = 100;

    auto fut = pool.submit(opts, []() -> int {
        throw std::runtime_error("task failed");
    });

    bool caught = false;
    try {
        fut.get();
    } catch (const std::runtime_error&) {
        caught = true;
    }

    assert(caught);

    pool.shutdown();
    std::cout << "test_submit_exception_with_opts passed\n";
}

// 测试 8：shutdown 后 submit(opts, ...) 应拒绝
void test_submit_after_shutdown_with_opts() {
    ThreadPool pool(2, PolicyType::FIFO);
    pool.start();
    pool.shutdown();

    ScheduleOptions opts;
    opts.priority = 10;

    bool caught = false;
    try {
        auto fut = pool.submit(opts, []() {
            return 1;
        });
        (void)fut;
    } catch (const std::logic_error&) {
        caught = true;
    }

    assert(caught);
    std::cout << "test_submit_after_shutdown_with_opts passed\n";
}

// 测试 9：多个带 opts 的 future
void test_multiple_submit_with_opts() {
    ThreadPool pool(4, PolicyType::FIFO);
    pool.start();

    std::vector<std::future<int>> futures;

    for (int i = 0; i < 10; ++i) {
        ScheduleOptions opts;
        opts.priority = i;  // 当前 FIFO 忽略，但接口通路要成立

        futures.push_back(pool.submit(opts, [i]() {
            return i * i;
        }));
    }

    for (int i = 0; i < 10; ++i) {
        assert(futures[i].get() == i * i);
    }

    pool.shutdown();
    std::cout << "test_multiple_submit_with_opts passed\n";
}

int main() {
    test_submit_task_default();
    test_submit_task_with_opts();
    test_submit_template_default();
    test_submit_template_with_opts();
    test_submit_lambda_with_opts();
    test_submit_void_lambda_with_opts();
    test_submit_exception_with_opts();
    test_submit_after_shutdown_with_opts();
    test_multiple_submit_with_opts();

    std::cout << "\nAll submit overload tests passed.\n";
    return 0;
}
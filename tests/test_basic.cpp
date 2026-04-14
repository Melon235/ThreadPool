#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "thread_pool/thread_pool.h"

using namespace thread_pool;

int main() {
    ThreadPool pool(4);

    // 测试初始状态
    if (pool.state() != PoolState::Created) {
        std::cout << "FAIL: initial state is not Created\n";
        return 1;
    }

    if (pool.thread_count() != 4) {
        std::cout << "FAIL: thread_count is incorrect\n";
        return 1;
    }

    pool.start();

    // 测试启动后状态
    if (pool.state() != PoolState::Running) {
        std::cout << "FAIL: state is not Running after start\n";
        return 1;
    }

    constexpr int task_count = 20;
    std::atomic<int> counter{0};

    for (int i = 0; i < task_count; ++i) {
        pool.submit([&counter]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ++counter;
        });
    }

    // 等待任务执行完成
    while (counter.load() < task_count) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (counter.load() != task_count) {
        std::cout << "FAIL: not all tasks were executed\n";
        return 1;
    }

    pool.shutdown();

    // 测试关闭后状态
    if (pool.state() != PoolState::Stopped) {
        std::cout << "FAIL: state is not Stopped after shutdown\n";
        return 1;
    }

    std::cout << "PASS: ThreadPool basic test\n";
    return 0;
}
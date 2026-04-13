#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "fifo_policy.h"

using namespace thread_pool;

int main() {
    // =========================
    // 测试 1：基础 enqueue / dequeue
    // =========================
    {
        FifoPolicy policy;
        int value = 0;

        policy.enqueue([&value]() {
            value = 42;
        });

        Task task = policy.dequeue(0);

        if (!task) {
            std::cout << "FAIL: basic test - dequeue returned empty task\n";
            return 1;
        }

        task();

        if (value != 42) {
            std::cout << "FAIL: basic test - task did not execute correctly\n";
            return 1;
        }

        std::cout << "PASS: basic enqueue/dequeue test\n";
    }

    // =========================
    // 测试 2：shutdown 唤醒等待线程
    // =========================
    {
        FifoPolicy policy;
        std::atomic<bool> got_empty_task{false};

        std::thread worker([&]() {
            Task task = policy.dequeue(0);
            if (!task) {
                got_empty_task = true;
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        policy.shutdown();
        worker.join();

        if (!got_empty_task.load()) {
            std::cout << "FAIL: shutdown test - dequeue did not return empty task\n";
            return 1;
        }

        std::cout << "PASS: shutdown wakeup test\n";
    }

    std::cout << "ALL TESTS PASSED\n";
    return 0;
}
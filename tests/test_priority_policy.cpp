#include "thread_pool/thread_pool.h"

#include <cassert>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

using namespace thread_pool;

namespace {

void test_fifo_policy_order() {
    ThreadPool pool(1, PolicyType::FIFO);

    std::vector<int> result;
    std::mutex result_mutex;

    pool.start();

    for (int i = 1; i <= 5; ++i) {
        pool.submit([&result, &result_mutex, i]() {
            std::lock_guard<std::mutex> lock(result_mutex);
            result.push_back(i);
        });
    }

    pool.shutdown();

    std::vector<int> expected{1, 2, 3, 4, 5};
    assert(result == expected);

    std::cout << "[PASS] FIFO policy executes in submit order\n";
}

void test_priority_policy_higher_first() {
    ThreadPool pool(1, PolicyType::PRIORITY);

    std::vector<std::string> result;
    std::mutex result_mutex;

    pool.start();

    pool.submit(ScheduleOptions{ScheduleOptions::npos, 0},
                [&result, &result_mutex]() {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result.push_back("low");
                });

    pool.submit(ScheduleOptions{ScheduleOptions::npos, 10},
                [&result, &result_mutex]() {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result.push_back("high");
                });

    pool.submit(ScheduleOptions{ScheduleOptions::npos, 5},
                [&result, &result_mutex]() {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result.push_back("mid");
                });

    pool.shutdown();

    std::vector<std::string> expected{"high", "mid", "low"};
    assert(result == expected);

    std::cout << "[PASS] Priority policy executes higher priority first\n";
}

void test_priority_policy_fifo_with_same_priority() {
    ThreadPool pool(1, PolicyType::PRIORITY);

    std::vector<int> result;
    std::mutex result_mutex;

    pool.start();

    for (int i = 1; i <= 5; ++i) {
        pool.submit(ScheduleOptions{ScheduleOptions::npos, 3},
                    [&result, &result_mutex, i]() {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        result.push_back(i);
                    });
    }

    pool.shutdown();

    std::vector<int> expected{1, 2, 3, 4, 5};
    assert(result == expected);

    std::cout << "[PASS] Priority policy keeps FIFO for same priority\n";
}

void test_priority_policy_mixed_order() {
    ThreadPool pool(1, PolicyType::PRIORITY);

    std::vector<std::string> result;
    std::mutex result_mutex;

    pool.start();

    pool.submit(ScheduleOptions{ScheduleOptions::npos, 0},
                [&result, &result_mutex]() {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result.push_back("A");
                });

    pool.submit(ScheduleOptions{ScheduleOptions::npos, 10},
                [&result, &result_mutex]() {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result.push_back("B");
                });

    pool.submit(ScheduleOptions{ScheduleOptions::npos, 10},
                [&result, &result_mutex]() {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result.push_back("C");
                });

    pool.submit(ScheduleOptions{ScheduleOptions::npos, 5},
                [&result, &result_mutex]() {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result.push_back("D");
                });

    pool.submit(ScheduleOptions{ScheduleOptions::npos, 0},
                [&result, &result_mutex]() {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result.push_back("E");
                });

    pool.shutdown();

    std::vector<std::string> expected{"B", "C", "D", "A", "E"};
    assert(result == expected);

    std::cout << "[PASS] Priority policy mixed order is correct\n";
}

}  // namespace

int main() {
    test_fifo_policy_order();
    test_priority_policy_higher_first();
    test_priority_policy_fifo_with_same_priority();
    test_priority_policy_mixed_order();

    std::cout << "\nAll policy tests passed.\n";
    return 0;
}

#include "thread_pool/thread_pool.h"

#include <cassert>
#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace tp = thread_pool;

namespace {

void test_fifo_batch() {
    constexpr std::size_t kThreadCount = 4;
    constexpr int kTaskCount = 120;

    tp::ThreadPool pool(kThreadCount, tp::PolicyType::FIFO);
    pool.start();

    std::vector<std::future<int>> futures;
    futures.reserve(kTaskCount);

    int expected_success_count = 0;
    long long expected_sum = 0;

    for (int i = 0; i < kTaskCount; ++i) {
        bool will_throw = (i % 17 == 0);

        futures.emplace_back(
            pool.submit([i, will_throw]() -> int {
                std::this_thread::sleep_for(std::chrono::milliseconds(12 + (i % 5) * 3));

                if (will_throw) {
                    throw std::runtime_error("mock task failure");
                }

                return i * i;
            })
        );

        if (!will_throw) {
            ++expected_success_count;
            expected_sum += 1LL * i * i;
        }
    }

    int success_count = 0;
    int fail_count = 0;
    long long actual_sum = 0;

    for (auto& fut : futures) {
        try {
            actual_sum += fut.get();
            ++success_count;
        } catch (const std::exception&) {
            ++fail_count;
        }
    }

    // 再补一批任务，验证前面出现异常后线程池仍然可用
    std::vector<std::future<int>> followup;
    for (int i = 0; i < 20; ++i) {
        followup.emplace_back(
            pool.submit([i]() -> int {
                std::this_thread::sleep_for(5ms);
                return i + 1000;
            })
        );
    }

    int followup_sum = 0;
    for (auto& fut : followup) {
        followup_sum += fut.get();
    }

    pool.shutdown();

    std::cout << "\n=== Business Test: FIFO Batch ===\n";
    std::cout << "task count        : " << kTaskCount << "\n";
    std::cout << "success count     : " << success_count << "\n";
    std::cout << "fail count        : " << fail_count << "\n";
    std::cout << "expected success  : " << expected_success_count << "\n";
    std::cout << "sum of results    : " << actual_sum << "\n";
    std::cout << "expected sum      : " << expected_sum << "\n";
    std::cout << "followup sum      : " << followup_sum << "\n";

    assert(success_count == expected_success_count);
    assert(fail_count == kTaskCount - expected_success_count);
    assert(actual_sum == expected_sum);

    // 1000 + 1001 + ... + 1019
    int expected_followup_sum = 0;
    for (int i = 0; i < 20; ++i) {
        expected_followup_sum += (i + 1000);
    }
    assert(followup_sum == expected_followup_sum);

    std::cout << "[PASS] FIFO batch scenario passed.\n";
}

} // namespace

int main() {
    test_fifo_batch();
    return 0;
}
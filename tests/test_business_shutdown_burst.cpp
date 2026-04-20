#include "thread_pool/thread_pool.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace tp = thread_pool;

namespace {

void test_shutdown_burst() {
    constexpr std::size_t kThreadCount = 4;
    constexpr int kProducerCount = 4;
    constexpr int kPerProducerSubmit = 80;

    tp::ThreadPool pool(kThreadCount, tp::PolicyType::FIFO);
    pool.start();

    std::mutex future_mutex;
    std::vector<std::future<int>> accepted_futures;

    std::atomic<int> accepted_count{0};
    std::atomic<int> rejected_count{0};
    std::atomic<int> executed_count{0};

    std::vector<std::thread> producers;
    producers.reserve(kProducerCount);

    for (int p = 0; p < kProducerCount; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < kPerProducerSubmit; ++i) {
                try {
                    auto fut = pool.submit([&, p, i]() -> int {
                        // 每 10 个任务混入一个较长任务
                        if (i % 10 == 0) {
                            std::this_thread::sleep_for(70ms);
                        } else {
                            std::this_thread::sleep_for(10ms);
                        }

                        executed_count.fetch_add(1, std::memory_order_relaxed);
                        return p * 100000 + i;
                    });

                    {
                        std::lock_guard<std::mutex> lock(future_mutex);
                        accepted_futures.push_back(std::move(fut));
                    }

                    accepted_count.fetch_add(1, std::memory_order_relaxed);

                    // 让 producer 更像真实突发流
                    std::this_thread::sleep_for(2ms);
                } catch (const std::exception&) {
                    rejected_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // 让系统运行一小段时间后触发 shutdown
    std::this_thread::sleep_for(120ms);
    pool.shutdown();

    for (auto& t : producers) {
        t.join();
    }

    int ready_count = 0;
    for (auto& fut : accepted_futures) {
        try {
            (void)fut.get();
            ++ready_count;
        } catch (...) {
            // 这里理论上不应该异常，若有也说明 accepted 的任务未正常完成
            assert(false && "accepted task future should not fail here");
        }
    }

    std::cout << "\n=== Business Test: Shutdown Burst ===\n";
    std::cout << "accepted count : " << accepted_count.load() << "\n";
    std::cout << "rejected count : " << rejected_count.load() << "\n";
    std::cout << "executed count : " << executed_count.load() << "\n";
    std::cout << "ready futures  : " << ready_count << "\n";

    // 核心一致性：所有成功提交的任务都必须执行完成并拿到 future
    assert(accepted_count.load() == executed_count.load());
    assert(accepted_count.load() == ready_count);

    // shutdown 中途触发后，通常应有一部分提交被拒绝
    assert(rejected_count.load() > 0);

    // 再尝试 shutdown 后提交，必须失败
    bool rejected_after_shutdown = false;
    try {
        auto fut = pool.submit([]() -> int { return 42; });
        (void)fut;
    } catch (const std::exception&) {
        rejected_after_shutdown = true;
    }

    assert(rejected_after_shutdown);

    std::cout << "[PASS] Shutdown burst scenario passed.\n";
}

} // namespace

int main() {
    test_shutdown_burst();
    return 0;
}
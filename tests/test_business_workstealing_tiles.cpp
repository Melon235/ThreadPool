#include "thread_pool/thread_pool.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace tp = thread_pool;

namespace {

struct TileResult {
    int tile_id = -1;
    bool is_heavy = false;
    bool is_subtask = false;
    std::thread::id tid;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point finish_time;
};

double ms_between(const std::chrono::steady_clock::time_point& a,
                  const std::chrono::steady_clock::time_point& b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

void test_workstealing_tiles() {
    constexpr std::size_t kThreadCount = 6;
    constexpr int kTileCount = 64;

    tp::ThreadPool pool(kThreadCount, tp::PolicyType::WORKSTEALING);
    pool.start();

    std::mutex result_mutex;
    std::vector<TileResult> results;

    std::atomic<int> root_done{0};
    std::atomic<int> sub_done{0};
    std::atomic<int> heavy_root_count{0};

    std::vector<std::future<void>> root_futures;
    root_futures.reserve(kTileCount);

    for (int i = 0; i < kTileCount; ++i) {
        root_futures.emplace_back(
            pool.submit([&, i]() {
                TileResult rec;
                rec.tile_id = i;
                rec.is_subtask = false;
                rec.start_time = std::chrono::steady_clock::now();
                rec.tid = std::this_thread::get_id();

                bool heavy = (i % 16 == 0) || (i % 16 == 7);
                rec.is_heavy = heavy;

                if (heavy) {
                    heavy_root_count.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::sleep_for(std::chrono::milliseconds(70 + (i % 3) * 15));

                    // 模拟复杂 tile 处理后继续拆子任务
                    for (int k = 0; k < 4; ++k) {
                        pool.submit([&, i, k]() {
                            TileResult sub;
                            sub.tile_id = i * 10 + k;
                            sub.is_heavy = false;
                            sub.is_subtask = true;
                            sub.start_time = std::chrono::steady_clock::now();
                            sub.tid = std::this_thread::get_id();

                            std::this_thread::sleep_for(std::chrono::milliseconds(18 + (k % 3) * 6));

                            sub.finish_time = std::chrono::steady_clock::now();

                            {
                                std::lock_guard<std::mutex> lock(result_mutex);
                                results.push_back(sub);
                            }

                            sub_done.fetch_add(1, std::memory_order_relaxed);
                        });
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10 + (i % 4) * 4));
                }

                rec.finish_time = std::chrono::steady_clock::now();

                {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    results.push_back(rec);
                }

                root_done.fetch_add(1, std::memory_order_relaxed);
            })
        );
    }

    for (auto& fut : root_futures) {
        fut.get();
    }

    // 等待子任务全部完成
    const int expected_subtasks = heavy_root_count.load() * 4;
    while (sub_done.load(std::memory_order_relaxed) < expected_subtasks) {
        std::this_thread::sleep_for(5ms);
    }

    pool.shutdown();

    assert(root_done.load() == kTileCount);
    assert(sub_done.load() == expected_subtasks);

    int root_count = 0;
    int sub_count = 0;
    std::map<std::thread::id, int> worker_distribution;
    double total_exec_ms = 0.0;

    for (const auto& r : results) {
        worker_distribution[r.tid]++;
        total_exec_ms += ms_between(r.start_time, r.finish_time);
        if (r.is_subtask) ++sub_count;
        else ++root_count;
    }

    std::cout << "\n=== Business Test: Work Stealing Tiles ===\n";
    std::cout << "root tasks finished: " << root_count << "\n";
    std::cout << "sub tasks finished : " << sub_count << "\n";
    std::cout << "heavy root tasks   : " << heavy_root_count.load() << "\n";
    std::cout << "expected subtasks  : " << expected_subtasks << "\n";
    std::cout << "worker distribution:\n";

    for (const auto& kv : worker_distribution) {
        std::cout << "  worker_tid=" << kv.first << " count=" << kv.second << "\n";
    }

    // 至少应有多个线程真正参与，避免“全压到一个线程”
    assert(worker_distribution.size() >= 3);

    // 结果数量必须对齐
    assert(root_count == kTileCount);
    assert(sub_count == expected_subtasks);

    std::cout << "[PASS] WorkStealing tile scenario passed.\n";
}

} // namespace

int main() {
    test_workstealing_tiles();
    return 0;
}
#include "thread_pool/thread_pool.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace tp = thread_pool;

namespace {

struct TaskRecord {
    int id = -1;
    int priority = 0;
    std::string type;

    std::chrono::steady_clock::time_point submit_time;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point finish_time;

    bool started = false;
    bool finished = false;
};

double ms_between(const std::chrono::steady_clock::time_point& a,
                  const std::chrono::steady_clock::time_point& b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

double average(const std::vector<double>& vals) {
    if (vals.empty()) return 0.0;
    double sum = std::accumulate(vals.begin(), vals.end(), 0.0);
    return sum / static_cast<double>(vals.size());
}

double percentile(std::vector<double> vals, double p) {
    if (vals.empty()) return 0.0;
    std::sort(vals.begin(), vals.end());
    std::size_t idx = static_cast<std::size_t>(p * (vals.size() - 1));
    return vals[idx];
}

void test_priority_service() {
    constexpr std::size_t kThreadCount = 4;
    constexpr int kTotalTasks = 80;

    tp::ThreadPool pool(kThreadCount, tp::PolicyType::PRIORITY);
    pool.start();

    std::mutex record_mutex;
    std::vector<TaskRecord> records;
    records.reserve(kTotalTasks);

    std::atomic<int> finished_count{0};

    auto submit_task = [&](int id, int priority, const std::string& type, int work_ms) {
        TaskRecord rec;
        rec.id = id;
        rec.priority = priority;
        rec.type = type;
        rec.submit_time = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(record_mutex);
            records.push_back(rec);
        }

        tp::ScheduleOptions opts;
        opts.priority = priority;

        pool.submit(
            [&, id, work_ms]() {
                {
                    std::lock_guard<std::mutex> lock(record_mutex);
                    for (auto& r : records) {
                        if (r.id == id) {
                            r.started = true;
                            r.start_time = std::chrono::steady_clock::now();
                            break;
                        }
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(work_ms));

                {
                    std::lock_guard<std::mutex> lock(record_mutex);
                    for (auto& r : records) {
                        if (r.id == id) {
                            r.finished = true;
                            r.finish_time = std::chrono::steady_clock::now();
                            break;
                        }
                    }
                }

                finished_count.fetch_add(1, std::memory_order_relaxed);
            },
            opts
        );
    };

    int id = 0;

    // 20 个低优先级后台任务
    for (int i = 0; i < 20; ++i) {
        submit_task(id++, 1, "low", 90 + (i % 3) * 10);
    }

    // 40 个中优先级普通请求
    for (int i = 0; i < 40; ++i) {
        submit_task(id++, 5, "mid", 25 + (i % 4) * 5);
    }

    // 20 个高优先级关键请求
    for (int i = 0; i < 20; ++i) {
        submit_task(id++, 10, "high", 8 + (i % 3) * 2);
    }

    // 等待全部任务完成
    while (finished_count.load(std::memory_order_relaxed) < kTotalTasks) {
        std::this_thread::sleep_for(2ms);
    }

    pool.shutdown();

    std::vector<double> high_wait, mid_wait, low_wait;
    std::vector<double> all_wait, all_exec;

    for (const auto& r : records) {
        assert(r.started);
        assert(r.finished);

        double wait_ms = ms_between(r.submit_time, r.start_time);
        double exec_ms = ms_between(r.start_time, r.finish_time);

        all_wait.push_back(wait_ms);
        all_exec.push_back(exec_ms);

        if (r.type == "high") {
            high_wait.push_back(wait_ms);
        } else if (r.type == "mid") {
            mid_wait.push_back(wait_ms);
        } else {
            low_wait.push_back(wait_ms);
        }
    }

    double high_avg = average(high_wait);
    double mid_avg = average(mid_wait);
    double low_avg = average(low_wait);

    std::cout << "\n=== Business Test: Priority Service ===\n";
    std::cout << "total tasks: " << records.size() << "\n";
    std::cout << "avg wait(ms): " << average(all_wait) << "\n";
    std::cout << "p95 wait(ms): " << percentile(all_wait, 0.95) << "\n";
    std::cout << "avg exec(ms): " << average(all_exec) << "\n";
    std::cout << "high avg wait(ms): " << high_avg << "\n";
    std::cout << "mid  avg wait(ms): " << mid_avg << "\n";
    std::cout << "low  avg wait(ms): " << low_avg << "\n";

    assert(high_avg < low_avg);
    assert(high_avg < mid_avg);

    std::cout << "[PASS] PriorityPolicy service scenario passed.\n";
}

} // namespace

int main() {
    test_priority_service();
    return 0;
}
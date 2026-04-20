#include <atomic>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <future>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "thread_pool/thread_pool.h"

using Clock = std::chrono::steady_clock;

namespace {

enum class BenchPolicy {
    FIFO,
    PRIORITY,
    WORKSTEALING
};

std::string policy_name(BenchPolicy p) {
    switch (p) {
        case BenchPolicy::FIFO: return "FIFO";
        case BenchPolicy::PRIORITY: return "PRIORITY";
        case BenchPolicy::WORKSTEALING: return "WORKSTEALING";
    }
    return "UNKNOWN";
}

thread_pool::ThreadPool create_pool(std::size_t thread_count, BenchPolicy policy) {
    using thread_pool::PolicyType;
    using thread_pool::ThreadPool;

    switch (policy) {
        case BenchPolicy::FIFO:
            return ThreadPool(thread_count, PolicyType::FIFO);
        case BenchPolicy::PRIORITY:
            return ThreadPool(thread_count, PolicyType::PRIORITY);
        case BenchPolicy::WORKSTEALING:
            return ThreadPool(thread_count, PolicyType::WORKSTEALING);
    }

    return ThreadPool(thread_count, PolicyType::FIFO);
}

template <class F>
void submit_normal_task(thread_pool::ThreadPool& pool, F&& f) {
    pool.submit(std::forward<F>(f));
}

template <class F>
void submit_priority_task(thread_pool::ThreadPool& pool, F&& f, int priority) {
    thread_pool::ScheduleOptions opts;
    opts.priority = priority;
    pool.submit(std::forward<F>(f), opts);
}

void busy_work(std::uint64_t rounds) {
    volatile std::uint64_t x = 0;
    for (std::uint64_t i = 0; i < rounds; ++i) {
        x += (i * 1315423911ull) ^ (x >> 3);
    }
    (void)x;
}

struct CsvRow {
    std::string case_name;
    std::string policy;
    std::size_t level = 0;
    std::size_t thread_count = 0;

    std::uint64_t task_count = 0;
    std::uint64_t work_rounds = 0;

    double total_seconds = 0.0;
    double throughput = 0.0;

    double high_done_ms = 0.0;
    double low_done_ms = 0.0;

    std::uint64_t extra_1 = 0;
    std::uint64_t extra_2 = 0;
};

void write_csv_header(std::ofstream& ofs) {
    ofs << "case_name,policy,level,thread_count,task_count,work_rounds,"
        << "total_seconds,throughput,high_done_ms,low_done_ms,extra_1,extra_2\n";
}

void write_csv_row(std::ofstream& ofs, const CsvRow& row) {
    ofs << row.case_name << ','
        << row.policy << ','
        << row.level << ','
        << row.thread_count << ','
        << row.task_count << ','
        << row.work_rounds << ','
        << std::fixed << std::setprecision(6) << row.total_seconds << ','
        << std::fixed << std::setprecision(6) << row.throughput << ','
        << std::fixed << std::setprecision(6) << row.high_done_ms << ','
        << std::fixed << std::setprecision(6) << row.low_done_ms << ','
        << row.extra_1 << ','
        << row.extra_2 << '\n';
}

CsvRow run_uniform_case(BenchPolicy policy, std::size_t thread_count, std::size_t level) {
    const std::uint64_t base_task_count = 20000;
    const std::uint64_t step_task_count = 20000;
    const std::uint64_t task_count = base_task_count + (level - 1) * step_task_count;

    const std::uint64_t base_rounds = 400;
    const std::uint64_t step_rounds = 40;
    const std::uint64_t work_rounds = base_rounds + (level - 1) * step_rounds;

    auto pool = create_pool(thread_count, policy);
    pool.start();

    std::atomic<std::uint64_t> done_count{0};
    std::promise<void> done_promise;
    std::future<void> done_future = done_promise.get_future();

    auto start = Clock::now();

    for (std::uint64_t i = 0; i < task_count; ++i) {
        auto task = [&done_count, &done_promise, task_count, work_rounds]() {
            busy_work(work_rounds);
            std::uint64_t finished = done_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (finished == task_count) {
                done_promise.set_value();
            }
        };

        if (policy == BenchPolicy::PRIORITY) {
            submit_priority_task(pool, task, 0);
        } else {
            submit_normal_task(pool, task);
        }
    }

    done_future.wait();
    auto end = Clock::now();

    pool.shutdown();

    CsvRow row;
    row.case_name = "uniform_throughput";
    row.policy = policy_name(policy);
    row.level = level;
    row.thread_count = thread_count;
    row.task_count = task_count;
    row.work_rounds = work_rounds;
    row.total_seconds = std::chrono::duration<double>(end - start).count();
    row.throughput = static_cast<double>(task_count) / row.total_seconds;
    return row;
}

CsvRow run_priority_case(BenchPolicy policy, std::size_t thread_count, std::size_t level) {
    const std::uint64_t low_task_count = 4000 + (level - 1) * 1200;
    const std::uint64_t high_task_count = 1000 + (level - 1) * 300;

    const std::uint64_t low_rounds = 15000 + (level - 1) * 1500;
    const std::uint64_t high_rounds = 300 + (level - 1) * 20;

    auto pool = create_pool(thread_count, policy);
    pool.start();

    std::atomic<std::uint64_t> low_done{0};
    std::atomic<std::uint64_t> high_done{0};

    std::promise<void> all_high_done_promise;
    std::future<void> all_high_done_future = all_high_done_promise.get_future();

    std::promise<void> all_low_done_promise;
    std::future<void> all_low_done_future = all_low_done_promise.get_future();

    auto start = Clock::now();

    for (std::uint64_t i = 0; i < low_task_count; ++i) {
        auto low_task = [&low_done, &all_low_done_promise, low_task_count, low_rounds]() {
            busy_work(low_rounds);
            std::uint64_t finished = low_done.fetch_add(1, std::memory_order_relaxed) + 1;
            if (finished == low_task_count) {
                all_low_done_promise.set_value();
            }
        };

        if (policy == BenchPolicy::PRIORITY) {
            submit_priority_task(pool, low_task, 1);
        } else {
            submit_normal_task(pool, low_task);
        }
    }

    for (std::uint64_t i = 0; i < high_task_count; ++i) {
        auto high_task = [&high_done, &all_high_done_promise, high_task_count, high_rounds]() {
            busy_work(high_rounds);
            std::uint64_t finished = high_done.fetch_add(1, std::memory_order_relaxed) + 1;
            if (finished == high_task_count) {
                all_high_done_promise.set_value();
            }
        };

        if (policy == BenchPolicy::PRIORITY) {
            submit_priority_task(pool, high_task, 100);
        } else {
            submit_normal_task(pool, high_task);
        }
    }

    all_high_done_future.wait();
    auto high_done_time = Clock::now();

    all_low_done_future.wait();
    auto low_done_time = Clock::now();

    pool.shutdown();

    CsvRow row;
    row.case_name = "priority_latency";
    row.policy = policy_name(policy);
    row.level = level;
    row.thread_count = thread_count;
    row.task_count = low_task_count + high_task_count;
    row.work_rounds = low_rounds;
    row.total_seconds = std::chrono::duration<double>(low_done_time - start).count();
    row.throughput = static_cast<double>(row.task_count) / row.total_seconds;
    row.high_done_ms = std::chrono::duration<double, std::milli>(high_done_time - start).count();
    row.low_done_ms = std::chrono::duration<double, std::milli>(low_done_time - start).count();
    row.extra_1 = high_task_count;
    row.extra_2 = low_task_count;
    return row;
}

struct RecursiveCaseState {
    thread_pool::ThreadPool* pool = nullptr;
    BenchPolicy policy = BenchPolicy::FIFO;
    int max_depth = 0;
    std::uint64_t rounds_per_node = 0;
    std::uint64_t total_nodes = 0;
    std::atomic<std::uint64_t> done_count{0};
    std::promise<void> done_promise;
};

CsvRow run_recursive_case(BenchPolicy policy, std::size_t thread_count, std::size_t level) {
    const int max_depth = 10 + static_cast<int>((level - 1) / 6);
    const std::uint64_t rounds_per_node = 600 + (level - 1) * 60;
    const std::uint64_t total_nodes = (1ULL << (max_depth + 1)) - 1;

    auto pool = create_pool(thread_count, policy);
    pool.start();

    auto state = std::make_shared<RecursiveCaseState>();
    state->pool = &pool;
    state->policy = policy;
    state->max_depth = max_depth;
    state->rounds_per_node = rounds_per_node;
    state->total_nodes = total_nodes;

    std::future<void> done_future = state->done_promise.get_future();
    auto runner = std::make_shared<std::function<void(int)>>();

    *runner = [state, runner](int depth) {
        busy_work(state->rounds_per_node);

        if (depth < state->max_depth) {
            auto left_task = [runner, depth]() {
                (*runner)(depth + 1);
            };
            auto right_task = [runner, depth]() {
                (*runner)(depth + 1);
            };

            if (state->policy == BenchPolicy::PRIORITY) {
                submit_priority_task(*(state->pool), left_task, state->max_depth - depth);
                submit_priority_task(*(state->pool), right_task, state->max_depth - depth);
            } else {
                submit_normal_task(*(state->pool), left_task);
                submit_normal_task(*(state->pool), right_task);
            }
        }

        std::uint64_t finished = state->done_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (finished == state->total_nodes) {
            state->done_promise.set_value();
        }
    };

    auto start = Clock::now();

    if (policy == BenchPolicy::PRIORITY) {
        submit_priority_task(pool, [runner]() { (*runner)(0); }, max_depth);
    } else {
        submit_normal_task(pool, [runner]() { (*runner)(0); });
    }

    CsvRow row;
    row.case_name = "recursive_spawn";
    row.policy = policy_name(policy);
    row.level = level;
    row.thread_count = thread_count;
    row.task_count = total_nodes;
    row.work_rounds = rounds_per_node;
    row.extra_1 = static_cast<std::uint64_t>(max_depth);
    row.extra_2 = total_nodes;

    auto status = done_future.wait_for(std::chrono::seconds(20));
    if (status != std::future_status::ready) {
        row.total_seconds = -1.0;
        row.throughput = -1.0;
        row.high_done_ms = 0.0;
        row.low_done_ms = 0.0;
        row.extra_1 = static_cast<std::uint64_t>(max_depth);
        row.extra_2 = state->done_count.load();
        pool.shutdown();
        return row;
    }

    auto end = Clock::now();
    pool.shutdown();

    row.total_seconds = std::chrono::duration<double>(end - start).count();
    row.throughput = static_cast<double>(total_nodes) / row.total_seconds;
    return row;
}

void run_case_group(std::ofstream& ofs,
                    const std::string& title,
                    CsvRow (*runner)(BenchPolicy, std::size_t, std::size_t),
                    std::size_t thread_count,
                    std::size_t levels) {
    const std::vector<BenchPolicy> policies = {
        BenchPolicy::FIFO,
        BenchPolicy::PRIORITY,
        BenchPolicy::WORKSTEALING
    };

    std::cout << "\n=== " << title << " ===\n";

    for (std::size_t level = 1; level <= levels; ++level) {
        std::cout << "[level " << std::setw(2) << level << "/" << levels << "]" << std::endl;

        for (BenchPolicy policy : policies) {
            CsvRow row = runner(policy, thread_count, level);
            write_csv_row(ofs, row);
            ofs.flush();

            if (row.case_name == "recursive_spawn" && row.total_seconds < 0.0) {
                std::cout << "  " << std::setw(15) << row.policy
                          << " | timeout"
                          << " | tasks=" << row.task_count
                          << " | rounds=" << row.work_rounds
                          << " | depth=" << row.extra_1
                          << " | done_count=" << row.extra_2
                          << std::endl;
            } else if (row.case_name == "priority_latency") {
                std::cout << "  " << std::setw(15) << row.policy
                          << " | tasks=" << row.task_count
                          << " | rounds=" << row.work_rounds
                          << " | total=" << std::fixed << std::setprecision(3) << row.total_seconds << " s"
                          << " | high_done=" << std::fixed << std::setprecision(2) << row.high_done_ms << " ms"
                          << " | low_done=" << std::fixed << std::setprecision(2) << row.low_done_ms << " ms"
                          << std::endl;
            } else {
                std::cout << "  " << std::setw(15) << row.policy
                          << " | tasks=" << row.task_count
                          << " | rounds=" << row.work_rounds
                          << " | time=" << std::fixed << std::setprecision(3) << row.total_seconds << " s"
                          << " | throughput=" << std::fixed << std::setprecision(2) << row.throughput
                          << std::endl;
            }
        }
    }
}

}  // namespace

int main() {
    std::size_t thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) {
        thread_count = 4;
    }

    const std::size_t levels = 40;
    const std::string csv_path = "build/tests/policy_benchmark_results.csv";

    std::ofstream ofs(csv_path);
    if (!ofs.is_open()) {
        std::cerr << "failed to open csv file: " << csv_path << "\n";
        return 1;
    }

    write_csv_header(ofs);

    std::cout << "thread_count = " << thread_count << std::endl;
    std::cout << "levels = " << levels << std::endl;
    std::cout << "csv = " << csv_path << std::endl;

    run_case_group(ofs, "uniform_throughput", run_uniform_case, thread_count, levels);
    run_case_group(ofs, "priority_latency", run_priority_case, thread_count, levels);
    run_case_group(ofs, "recursive_spawn", run_recursive_case, thread_count, levels);

    ofs.close();

    std::cout << "\nDone. CSV written to: " << csv_path << std::endl;
    return 0;
}
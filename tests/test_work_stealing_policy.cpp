#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "thread_pool/thread_pool.h"

using Clock = std::chrono::steady_clock;

namespace {

void busy_work(std::uint64_t rounds) {
    volatile std::uint64_t x = 0;
    for (std::uint64_t i = 0; i < rounds; ++i) {
        x += (i * 1315423911ull) ^ (x >> 3);
    }
    (void)x;
}

struct SharedState {
    std::atomic<std::uint64_t> submitted{0};
    std::atomic<std::uint64_t> completed{0};
    std::atomic<std::uint64_t> in_flight{0};

    std::atomic<std::uint64_t> root_submitted{0};
    std::atomic<std::uint64_t> producer_submitted{0};
    std::atomic<std::uint64_t> spawned_submitted{0};

    std::atomic<std::uint64_t> submit_failures{0};
};

void maybe_finish(const std::shared_ptr<SharedState>& state, std::promise<void>& done_promise) {
    std::uint64_t remaining = state->in_flight.fetch_sub(1, std::memory_order_acq_rel) - 1;
    state->completed.fetch_add(1, std::memory_order_relaxed);

    if (remaining == 0) {
        try {
            done_promise.set_value();
        } catch (...) {
        }
    }
}

void submit_wrapped_task(
    thread_pool::ThreadPool& pool,
    const std::shared_ptr<SharedState>& state,
    std::promise<void>& done_promise,
    std::function<void()> body
) {
    state->submitted.fetch_add(1, std::memory_order_relaxed);
    state->in_flight.fetch_add(1, std::memory_order_relaxed);

    pool.submit([state, &done_promise, body = std::move(body)]() mutable {
        body();
        maybe_finish(state, done_promise);
    });
}

bool run_one_round(std::size_t round_index, std::size_t thread_count) {
    std::cerr << "[DBG] round " << round_index << " enter" << std::endl;

    std::cerr << "[DBG] round " << round_index << " before pool ctor" << std::endl;
    thread_pool::ThreadPool pool(thread_count, thread_pool::PolicyType::WORKSTEALING);
    std::cerr << "[DBG] round " << round_index << " after pool ctor" << std::endl;

    std::cerr << "[DBG] round " << round_index << " before start" << std::endl;
    pool.start();
    std::cerr << "[DBG] round " << round_index << " after start" << std::endl;

    auto state = std::make_shared<SharedState>();
    std::promise<void> done_promise;
    std::future<void> done_future = done_promise.get_future();

    const int root_task_count = 64;
    const int producer_thread_count = 4;
    const int tasks_per_producer = 500;
    const int max_spawn_depth = 7;

    auto runner = std::make_shared<std::function<void(int, std::uint32_t)>>();

    *runner = [&pool, state, &done_promise, runner, max_spawn_depth](int depth, std::uint32_t seed) {
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> work_dist(200, 1200);
        std::uniform_int_distribution<int> child_count_dist(0, 2);
        std::uniform_int_distribution<int> extra_delay_dist(0, 9);

        busy_work(static_cast<std::uint64_t>(work_dist(rng)));

        int delay_pick = extra_delay_dist(rng);
        if (delay_pick == 0) {
            std::this_thread::yield();
        }

        if (depth >= max_spawn_depth) {
            return;
        }

        int child_count = child_count_dist(rng);

        for (int i = 0; i < child_count; ++i) {
            std::uint32_t child_seed =
                seed * 1664525u + 1013904223u + static_cast<std::uint32_t>(depth * 17 + i);

            try {
                state->spawned_submitted.fetch_add(1, std::memory_order_relaxed);

                submit_wrapped_task(
                    pool,
                    state,
                    done_promise,
                    [runner, depth, child_seed]() {
                        (*runner)(depth + 1, child_seed);
                    }
                );
            } catch (...) {
                state->submit_failures.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::cerr << "[DBG] round " << round_index << " before root submit loop" << std::endl;
    for (int i = 0; i < root_task_count; ++i) {
        std::uint32_t seed = 12345u + static_cast<std::uint32_t>(i * 97);

        state->root_submitted.fetch_add(1, std::memory_order_relaxed);

        submit_wrapped_task(
            pool,
            state,
            done_promise,
            [runner, seed]() {
                (*runner)(0, seed);
            }
        );
    }
    std::cerr << "[DBG] round " << round_index << " after root submit loop" << std::endl;

    std::vector<std::thread> producers;
    producers.reserve(producer_thread_count);

    std::cerr << "[DBG] round " << round_index << " before producer create" << std::endl;
    for (int p = 0; p < producer_thread_count; ++p) {
        producers.emplace_back([p, tasks_per_producer, &pool, state, &done_promise]() {
            std::mt19937 rng(9000u + static_cast<std::uint32_t>(p * 131));
            std::uniform_int_distribution<int> work_dist(100, 600);
            std::uniform_int_distribution<int> yield_dist(0, 19);

            for (int i = 0; i < tasks_per_producer; ++i) {
                try {
                    state->producer_submitted.fetch_add(1, std::memory_order_relaxed);

                    submit_wrapped_task(
                        pool,
                        state,
                        done_promise,
                        [rng_seed = rng(), work = work_dist(rng)]() mutable {
                            busy_work(static_cast<std::uint64_t>(work));

                            std::mt19937 local_rng(rng_seed);
                            if ((local_rng() % 11) == 0) {
                                std::this_thread::yield();
                            }
                        }
                    );
                } catch (...) {
                    state->submit_failures.fetch_add(1, std::memory_order_relaxed);
                }

                if (yield_dist(rng) == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }
    std::cerr << "[DBG] round " << round_index << " after producer create" << std::endl;

    std::cerr << "[DBG] round " << round_index << " before producer join" << std::endl;
    for (auto& t : producers) {
        t.join();
    }
    std::cerr << "[DBG] round " << round_index << " after producer join" << std::endl;

    std::cerr << "[DBG] round " << round_index << " before wait_for" << std::endl;
    auto status = done_future.wait_for(std::chrono::seconds(10));
    std::cerr << "[DBG] round " << round_index << " after wait_for" << std::endl;

    if (status != std::future_status::ready) {
        std::cerr
            << "[TIMEOUT] round=" << round_index
            << " | submitted=" << state->submitted.load()
            << " | completed=" << state->completed.load()
            << " | in_flight=" << state->in_flight.load()
            << " | root_submitted=" << state->root_submitted.load()
            << " | producer_submitted=" << state->producer_submitted.load()
            << " | spawned_submitted=" << state->spawned_submitted.load()
            << " | submit_failures=" << state->submit_failures.load()
            << std::endl;

        std::cerr << "[DBG] round " << round_index << " before shutdown(timeout)" << std::endl;
        pool.shutdown();
        std::cerr << "[DBG] round " << round_index << " after shutdown(timeout)" << std::endl;
        return false;
    }

    std::cerr << "[DBG] round " << round_index << " before shutdown(ok)" << std::endl;
    pool.shutdown();
    std::cerr << "[DBG] round " << round_index << " after shutdown(ok)" << std::endl;

    std::cout
        << "[OK] round=" << std::setw(3) << round_index
        << " | submitted=" << state->submitted.load()
        << " | completed=" << state->completed.load()
        << " | root=" << state->root_submitted.load()
        << " | producer=" << state->producer_submitted.load()
        << " | spawned=" << state->spawned_submitted.load()
        << " | submit_failures=" << state->submit_failures.load()
        << std::endl;

    std::cerr << "[DBG] round " << round_index << " before return" << std::endl;
    return true;
}

}  // namespace

int main() {
    std::size_t thread_count = std::thread::hardware_concurrency();
    if (thread_count == 0) {
        thread_count = 4;
    }

    const int rounds = 100;

    std::cout << "thread_count = " << thread_count << "\n";
    std::cout << "rounds = " << rounds << "\n";
    std::cout << "policy = WORKSTEALING\n";
    std::cout << "============================================================\n";

    for (int round = 1; round <= rounds; ++round) {
        std::cerr << "[DBG] main before run_one_round(" << round << ")" << std::endl;
        bool ok = run_one_round(static_cast<std::size_t>(round), thread_count);
        std::cerr << "[DBG] main after run_one_round(" << round << ")" << std::endl;

        if (!ok) {
            std::cerr << "Reproduced bug at round " << round << std::endl;
            return 1;
        }
    }

    std::cout << "============================================================\n";
    std::cout << "All rounds passed.\n";
    return 0;
}
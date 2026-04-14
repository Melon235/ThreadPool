#include "thread_pool/thread_pool.h"

#include <atomic>
#include <cassert>
#include <future>
#include <iostream>
#include <stdexcept>
#include <vector>

using thread_pool::ThreadPool;

int add(int a, int b) {
    return a + b;
}

void test_return_int() {
    ThreadPool pool(2);
    pool.start();

    auto fut = pool.submit(add, 1, 2);
    assert(fut.get() == 3);

    pool.shutdown();
    std::cout << "test_return_int passed\n";
}

void test_return_lambda() {
    ThreadPool pool(2);
    pool.start();

    auto fut = pool.submit([]() { return 42; });
    assert(fut.get() == 42);

    pool.shutdown();
    std::cout << "test_return_lambda passed\n";
}

void test_return_void() {
    ThreadPool pool(2);
    pool.start();

    std::atomic<int> counter{0};

    auto fut = pool.submit([&counter]() {
        counter.fetch_add(1);
    });

    fut.get();
    assert(counter.load() == 1);

    pool.shutdown();
    std::cout << "test_return_void passed\n";
}

void test_multiple_futures() {
    ThreadPool pool(4);
    pool.start();

    std::vector<std::future<int>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([i]() { return i * i; }));
    }

    for (int i = 0; i < 10; ++i) {
        assert(futures[i].get() == i * i);
    }

    pool.shutdown();
    std::cout << "test_multiple_futures passed\n";
}

void test_exception_propagation() {
    ThreadPool pool(2);
    pool.start();

    auto fut = pool.submit([]() -> int {
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
    std::cout << "test_exception_propagation passed\n";
}

void test_submit_after_shutdown() {
    ThreadPool pool(2);
    pool.start();
    pool.shutdown();

    bool caught = false;
    try {
        auto fut = pool.submit([]() { return 1; });
        (void)fut;
    } catch (const std::logic_error&) {
        caught = true;
    }

    assert(caught);
    std::cout << "test_submit_after_shutdown passed\n";
}

int main() {
    test_return_int();
    test_return_lambda();
    test_return_void();
    test_multiple_futures();
    test_exception_propagation();
    test_submit_after_shutdown();

    std::cout << "\nAll future tests passed.\n";
    return 0;
}
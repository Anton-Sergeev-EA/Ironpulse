#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <numeric>
#include <vector>

#include "ironpulse/core/thread_pool.hpp"

using ironpulse::core::ThreadPool;

TEST_CASE("ThreadPool executes a single task and returns its result", "[thread_pool]") {
    ThreadPool pool(2);
    auto future = pool.submit([] { return 21 * 2; });
    CHECK(future.get() == 42);
}

TEST_CASE("ThreadPool executes many tasks concurrently without losing any", "[thread_pool]") {
    ThreadPool pool(4);
    constexpr int kTaskCount = 1000;
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    futures.reserve(kTaskCount);
    for (int i = 0; i < kTaskCount; ++i) {
        futures.push_back(pool.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); }));
    }
    for (auto& f : futures) f.get();

    CHECK(counter.load() == kTaskCount);
}

TEST_CASE("ThreadPool propagates exceptions through the future", "[thread_pool]") {
    ThreadPool pool(1);
    auto future = pool.submit([]() -> int { throw std::runtime_error("boom"); });
    CHECK_THROWS_AS(future.get(), std::runtime_error);
}

TEST_CASE("ThreadPool rejects new work after shutdown", "[thread_pool]") {
    ThreadPool pool(1);
    pool.shutdown();
    CHECK_THROWS_AS(pool.submit([] {}), std::runtime_error);
}

#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace ironpulse::core {

/// A simple, fixed-size thread pool with a work-stealing-free FIFO queue.
///
/// Usage:
///   ThreadPool pool{4};
///   auto fut = pool.submit([]{ return 42; });
///   int result = fut.get();
class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count = std::thread::hardware_concurrency()) {
        if (thread_count == 0) {
            thread_count = 1;
        }
        workers_.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        shutdown();
    }

    /// Submits a callable and returns a future to its result.
    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<ReturnType> result = task->get_future();

        {
            std::lock_guard lock(queue_mutex_);
            if (stopping_) {
                throw std::runtime_error("submit() called on a stopped ThreadPool");
            }
            tasks_.emplace([task] { (*task)(); });
        }
        cv_.notify_one();
        return result;
    }

    /// Requests all workers to finish pending tasks and join.
    void shutdown() noexcept {
        {
            std::lock_guard lock(queue_mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return workers_.size();
    }

private:
    void worker_loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock(queue_mutex_);
                cv_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    bool stopping_ = false;
};

}  // namespace ironpulse::core

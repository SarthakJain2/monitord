#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>

namespace http {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Non-copyable, non-movable (mutex is not movable)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) noexcept = delete;
    ThreadPool& operator=(ThreadPool&&) noexcept = delete;

    // Enqueue a task
    template<typename F, typename... Args>
    auto Enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type>;

    size_t Size() const { return threads_.size(); }
    size_t PendingTasks() const;

private:
    void WorkerThread();

    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};

template<typename F, typename... Args>
auto ThreadPool::Enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> result = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (stop_) {
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        }
        tasks_.emplace([task](){ (*task)(); });
    }
    condition_.notify_one();
    return result;
}

} // namespace http

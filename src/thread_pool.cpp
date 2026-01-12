#include "thread_pool.hpp"
#include <stdexcept>

namespace http {

ThreadPool::ThreadPool(size_t num_threads) : stop_(false) {
    if (num_threads == 0) {
        num_threads = 1;
    }
    
    for (size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    
    for (std::thread& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

size_t ThreadPool::PendingTasks() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return tasks_.size();
}

void ThreadPool::WorkerThread() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });
            
            if (stop_ && tasks_.empty()) {
                return;
            }
            
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        task();
    }
}

} // namespace http

#include <gtest/gtest.h>
#include "thread_pool.hpp"
#include <thread>
#include <chrono>
#include <atomic>

using namespace http;

TEST(ThreadPoolTest, ExecuteTask) {
    ThreadPool pool(2);
    
    std::atomic<int> counter{0};
    
    auto future = pool.Enqueue([&counter]() {
        counter = 42;
        return counter.load();
    });
    
    int result = future.get();
    
    EXPECT_EQ(result, 42);
    EXPECT_EQ(counter.load(), 42);
}

TEST(ThreadPoolTest, MultipleTasks) {
    ThreadPool pool(4);
    
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.Enqueue([&counter]() {
            counter++;
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_EQ(counter.load(), 10);
}

TEST(ThreadPoolTest, ReturnValues) {
    ThreadPool pool(2);
    
    auto future1 = pool.Enqueue([]() { return 1; });
    auto future2 = pool.Enqueue([]() { return std::string("test"); });
    auto future3 = pool.Enqueue([]() { return 3.14; });
    
    EXPECT_EQ(future1.get(), 1);
    EXPECT_EQ(future2.get(), "test");
    EXPECT_DOUBLE_EQ(future3.get(), 3.14);
}

TEST(ThreadPoolTest, ConcurrentExecution) {
    ThreadPool pool(4);
    
    std::atomic<int> active_tasks{0};
    std::atomic<int> max_concurrent{0};
    
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < 20; ++i) {
        futures.push_back(pool.Enqueue([&active_tasks, &max_concurrent]() {
            int current = ++active_tasks;
            int max = max_concurrent.load();
            while (current > max && !max_concurrent.compare_exchange_weak(max, current)) {
                max = max_concurrent.load();
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            --active_tasks;
        }));
    }
    
    for (auto& future : futures) {
        future.wait();
    }
    
    EXPECT_GT(max_concurrent.load(), 1);
    EXPECT_LE(max_concurrent.load(), 4);
}

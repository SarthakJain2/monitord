#include "server.hpp"
#include "http_response.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <string>

using namespace http;
using namespace std::chrono;

class BenchmarkClient {
public:
    BenchmarkClient(const std::string& host, uint16_t port) 
        : host_(host), port_(port) {}
    
    bool Connect() {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) return false;
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);
        
        return connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0;
    }
    
    bool SendRequest(const std::string& request) {
        return send(fd_, request.c_str(), request.length(), 0) > 0;
    }
    
    std::string ReceiveResponse() {
        char buffer[8192];
        ssize_t received = recv(fd_, buffer, sizeof(buffer) - 1, 0);
        if (received > 0) {
            buffer[received] = '\0';
            return std::string(buffer);
        }
        return "";
    }
    
    void Close() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }
    
    ~BenchmarkClient() {
        Close();
    }
    
private:
    int fd_ = -1;
    std::string host_;
    uint16_t port_;
};

void RunBenchmark(const std::string& name, int num_requests, int num_threads) {
    Config config;
    config.port = 8888;
    config.thread_pool_size = 4;
    
    Server server(config);
    
    server.Get("/bench", [](const HttpRequest& req) {
        return JsonResponse(R"({"status": "ok", "message": "benchmark response"})");
    });
    
    // Start server in background
    std::thread server_thread([&server]() {
        server.Start();
    });
    
    // Wait for server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    
    auto start_time = high_resolution_clock::now();
    
    std::vector<std::thread> client_threads;
    
    for (int t = 0; t < num_threads; ++t) {
        client_threads.emplace_back([&, t]() {
            int requests_per_thread = num_requests / num_threads;
            
            for (int i = 0; i < requests_per_thread; ++i) {
                BenchmarkClient client("127.0.0.1", 8888);
                
                if (client.Connect()) {
                    std::string request = 
                        "GET /bench HTTP/1.1\r\n"
                        "Host: localhost:8888\r\n"
                        "\r\n";
                    
                    if (client.SendRequest(request)) {
                        std::string response = client.ReceiveResponse();
                        if (!response.empty() && response.find("200 OK") != std::string::npos) {
                            success_count++;
                        } else {
                            failure_count++;
                        }
                    } else {
                        failure_count++;
                    }
                    
                    client.Close();
                } else {
                    failure_count++;
                }
            }
        });
    }
    
    for (auto& thread : client_threads) {
        thread.join();
    }
    
    auto end_time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end_time - start_time).count();
    
    server.Stop();
    server_thread.join();
    
    double requests_per_sec = (success_count.load() * 1000.0) / duration;
    
    std::cout << "\n=== " << name << " ===\n";
    std::cout << "Total requests: " << num_requests << "\n";
    std::cout << "Successful: " << success_count.load() << "\n";
    std::cout << "Failed: " << failure_count.load() << "\n";
    std::cout << "Duration: " << duration << " ms\n";
    std::cout << "Throughput: " << requests_per_sec << " req/s\n";
    std::cout << "Average latency: " << (duration * 1.0 / num_requests) << " ms\n";
}

int main() {
    std::cout << "Running HTTP Server Benchmarks\n";
    std::cout << "==============================\n\n";
    
    // Warm-up
    std::cout << "Warming up...\n";
    RunBenchmark("Warm-up", 100, 1);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Single-threaded benchmark
    RunBenchmark("Single-threaded (1000 requests)", 1000, 1);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // Multi-threaded benchmark
    RunBenchmark("Multi-threaded (1000 requests, 10 threads)", 1000, 10);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // High-load benchmark
    RunBenchmark("High-load (5000 requests, 20 threads)", 5000, 20);
    
    return 0;
}

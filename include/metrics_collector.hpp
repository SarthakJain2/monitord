#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <cstdint>

namespace http {

struct SystemMetrics {
    // CPU metrics
    double cpu_percent;
    double cpu_user;
    double cpu_system;
    double cpu_idle;
    
    // Memory metrics
    uint64_t memory_total;      // bytes
    uint64_t memory_used;        // bytes
    uint64_t memory_free;        // bytes
    double memory_percent;
    
    // Disk metrics
    uint64_t disk_total;         // bytes
    uint64_t disk_used;          // bytes
    uint64_t disk_free;          // bytes
    double disk_percent;
    
    // Network metrics (cumulative totals since boot)
    uint64_t network_rx_bytes;   // total received bytes
    uint64_t network_tx_bytes;   // total transmitted bytes
    uint64_t network_rx_packets;  // total received packets
    uint64_t network_tx_packets; // total transmitted packets
    double network_rx_rate;      // current RX rate (bytes/sec)
    double network_tx_rate;       // current TX rate (bytes/sec)
    
    // Timestamp
    std::chrono::system_clock::time_point timestamp;
    
    // Convert to JSON
    std::string ToJson() const;
};

class MetricsCollector {
public:
    MetricsCollector();
    ~MetricsCollector() = default;
    
    // Non-copyable, non-movable
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;
    MetricsCollector(MetricsCollector&&) = delete;
    MetricsCollector& operator=(MetricsCollector&&) = delete;
    
    // Collect current system metrics
    SystemMetrics Collect();
    
    // Get CPU usage (returns percentage)
    double GetCpuUsage();
    
    // Get memory usage
    void GetMemoryUsage(uint64_t& total, uint64_t& used, uint64_t& free);
    
    // Get disk usage for root filesystem
    void GetDiskUsage(uint64_t& total, uint64_t& used, uint64_t& free);
    
    // Get network statistics
    void GetNetworkStats(uint64_t& rx_bytes, uint64_t& tx_bytes,
                        uint64_t& rx_packets, uint64_t& tx_packets,
                        double& rx_rate, double& tx_rate);

private:
    // Previous CPU stats for calculating usage
    uint64_t prev_user_time_;
    uint64_t prev_system_time_;
    uint64_t prev_idle_time_;
    std::chrono::system_clock::time_point prev_cpu_time_;
    bool first_collection_;
    
    // Previous network stats
    uint64_t prev_rx_bytes_;
    uint64_t prev_tx_bytes_;
    uint64_t prev_rx_packets_;
    uint64_t prev_tx_packets_;
    std::chrono::system_clock::time_point prev_network_time_;
    bool first_network_collection_;
    
    // Helper methods for macOS
    void ReadCpuTimes(uint64_t& user, uint64_t& system, uint64_t& idle, uint64_t& total);
    void ReadMemoryStats(uint64_t& total, uint64_t& used, uint64_t& free);
    void ReadDiskStats(uint64_t& total, uint64_t& used, uint64_t& free);
    void ReadNetworkStats(uint64_t& rx_bytes, uint64_t& tx_bytes,
                         uint64_t& rx_packets, uint64_t& tx_packets);
};

} // namespace http

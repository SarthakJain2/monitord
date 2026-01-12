#pragma once

#include "metrics_collector.hpp"
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <algorithm>

namespace http {

class MetricsStorage {
public:
    explicit MetricsStorage(size_t max_samples = 3600); // Default: 1 hour at 1 sample/sec
    ~MetricsStorage() = default;
    
    // Non-copyable, non-movable
    MetricsStorage(const MetricsStorage&) = delete;
    MetricsStorage& operator=(const MetricsStorage&) = delete;
    MetricsStorage(MetricsStorage&&) = delete;
    MetricsStorage& operator=(MetricsStorage&&) = delete;
    
    // Add a new metrics sample
    void AddSample(const SystemMetrics& metrics);
    
    // Get all samples within a time range
    std::vector<SystemMetrics> GetSamples(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end) const;
    
    // Get the most recent N samples
    std::vector<SystemMetrics> GetRecentSamples(size_t count) const;
    
    // Get the latest sample
    SystemMetrics GetLatest() const;
    
    // Get samples for the last N seconds
    std::vector<SystemMetrics> GetLastSeconds(size_t seconds) const;
    
    // Get aggregated statistics
    struct AggregatedStats {
        double avg_cpu;
        double max_cpu;
        double min_cpu;
        double avg_memory;
        double max_memory;
        double min_memory;
        uint64_t total_network_rx;
        uint64_t total_network_tx;
    };
    
    AggregatedStats GetAggregatedStats(
        std::chrono::system_clock::time_point start,
        std::chrono::system_clock::time_point end) const;
    
    // Clear old samples (called automatically, but can be called manually)
    void Cleanup();
    
    size_t GetSampleCount() const;

private:
    mutable std::mutex mutex_;
    std::deque<SystemMetrics> samples_;
    size_t max_samples_;
    
    void RemoveOldSamples();
};

} // namespace http

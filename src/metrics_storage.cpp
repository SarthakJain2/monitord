#include "metrics_storage.hpp"
#include <algorithm>

namespace http {

MetricsStorage::MetricsStorage(size_t max_samples)
    : max_samples_(max_samples) {
}

void MetricsStorage::AddSample(const SystemMetrics& metrics) {
    std::lock_guard<std::mutex> lock(mutex_);
    samples_.push_back(metrics);
    
    if (samples_.size() > max_samples_) {
        samples_.pop_front();
    }
}

std::vector<SystemMetrics> MetricsStorage::GetSamples(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SystemMetrics> result;
    
    for (const auto& sample : samples_) {
        if (sample.timestamp >= start && sample.timestamp <= end) {
            result.push_back(sample);
        }
    }
    
    return result;
}

std::vector<SystemMetrics> MetricsStorage::GetRecentSamples(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SystemMetrics> result;
    
    size_t start_idx = (samples_.size() > count) ? samples_.size() - count : 0;
    for (size_t i = start_idx; i < samples_.size(); ++i) {
        result.push_back(samples_[i]);
    }
    
    return result;
}

SystemMetrics MetricsStorage::GetLatest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (samples_.empty()) {
        return SystemMetrics{};
    }
    
    return samples_.back();
}

std::vector<SystemMetrics> MetricsStorage::GetLastSeconds(size_t seconds) const {
    auto now = std::chrono::system_clock::now();
    auto start = now - std::chrono::seconds(seconds);
    
    return GetSamples(start, now);
}

MetricsStorage::AggregatedStats MetricsStorage::GetAggregatedStats(
    std::chrono::system_clock::time_point start,
    std::chrono::system_clock::time_point end) const {
    
    auto samples = GetSamples(start, end);
    AggregatedStats stats{};
    
    if (samples.empty()) {
        return stats;
    }
    
    double cpu_sum = 0.0, memory_sum = 0.0;
    double cpu_max = 0.0, cpu_min = 100.0;
    double memory_max = 0.0, memory_min = 100.0;
    uint64_t network_rx_start = 0, network_tx_start = 0;
    uint64_t network_rx_end = 0, network_tx_end = 0;
    
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        
        cpu_sum += s.cpu_percent;
        memory_sum += s.memory_percent;
        
        cpu_max = std::max(cpu_max, s.cpu_percent);
        cpu_min = std::min(cpu_min, s.cpu_percent);
        memory_max = std::max(memory_max, s.memory_percent);
        memory_min = std::min(memory_min, s.memory_percent);
        
        if (i == 0) {
            network_rx_start = s.network_rx_bytes;
            network_tx_start = s.network_tx_bytes;
        }
        if (i == samples.size() - 1) {
            network_rx_end = s.network_rx_bytes;
            network_tx_end = s.network_tx_bytes;
        }
    }
    
    stats.avg_cpu = cpu_sum / samples.size();
    stats.max_cpu = cpu_max;
    stats.min_cpu = cpu_min;
    stats.avg_memory = memory_sum / samples.size();
    stats.max_memory = memory_max;
    stats.min_memory = memory_min;
    stats.total_network_rx = (network_rx_end > network_rx_start) ? network_rx_end - network_rx_start : 0;
    stats.total_network_tx = (network_tx_end > network_tx_start) ? network_tx_end - network_tx_start : 0;
    
    return stats;
}

void MetricsStorage::Cleanup() {
    RemoveOldSamples();
}

size_t MetricsStorage::GetSampleCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return samples_.size();
}

void MetricsStorage::RemoveOldSamples() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    while (samples_.size() > max_samples_) {
        samples_.pop_front();
    }
}

} // namespace http

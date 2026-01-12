#pragma once

#include "metrics_collector.hpp"
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>

namespace http {

enum class AlertType {
    CPU_HIGH,
    MEMORY_HIGH,
    DISK_HIGH,
    NETWORK_HIGH
};

struct Alert {
    AlertType type;
    std::string message;
    double threshold;
    double current_value;
    std::chrono::system_clock::time_point timestamp;
    bool active;
    
    std::string ToJson() const;
};

using AlertCallback = std::function<void(const Alert&)>;

class AlertManager {
public:
    AlertManager();
    ~AlertManager() = default;
    
    // Non-copyable, non-movable
    AlertManager(const AlertManager&) = delete;
    AlertManager& operator=(const AlertManager&) = delete;
    AlertManager(AlertManager&&) = delete;
    AlertManager& operator=(AlertManager&&) = delete;
    
    // Set thresholds
    void SetCpuThreshold(double threshold) { cpu_threshold_ = threshold; }
    void SetMemoryThreshold(double threshold) { memory_threshold_ = threshold; }
    void SetDiskThreshold(double threshold) { disk_threshold_ = threshold; }
    void SetNetworkThreshold(uint64_t threshold_bytes_per_sec) { network_threshold_ = threshold_bytes_per_sec; }
    
    // Register alert callback
    void SetAlertCallback(AlertCallback callback) { alert_callback_ = callback; }
    
    // Check metrics and trigger alerts if needed
    void CheckMetrics(const SystemMetrics& metrics);
    
    // Get all active alerts
    std::vector<Alert> GetActiveAlerts() const;
    
    // Get alert history
    std::vector<Alert> GetAlertHistory(size_t count = 100) const;
    
    // Clear resolved alerts
    void ClearResolvedAlerts();

private:
    mutable std::mutex mutex_;
    double cpu_threshold_;
    double memory_threshold_;
    double disk_threshold_;
    uint64_t network_threshold_;
    
    std::vector<Alert> active_alerts_;
    std::vector<Alert> alert_history_;
    
    AlertCallback alert_callback_;
    
    // Previous network stats for rate calculation
    uint64_t prev_network_rx_;
    uint64_t prev_network_tx_;
    std::chrono::system_clock::time_point prev_network_time_;
    bool first_check_;
    
    void TriggerAlert(AlertType type, const std::string& message, double threshold, double current_value);
    void ResolveAlert(AlertType type);
    bool IsAlertActive(AlertType type) const;
};

} // namespace http

#include "alert_manager.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace http {

std::string AlertTypeToString(AlertType type) {
    switch (type) {
        case AlertType::CPU_HIGH: return "CPU_HIGH";
        case AlertType::MEMORY_HIGH: return "MEMORY_HIGH";
        case AlertType::DISK_HIGH: return "DISK_HIGH";
        case AlertType::NETWORK_HIGH: return "NETWORK_HIGH";
        default: return "UNKNOWN";
    }
}

} // namespace http

namespace http {

AlertManager::AlertManager()
    : cpu_threshold_(80.0),
      memory_threshold_(85.0),
      disk_threshold_(90.0),
      network_threshold_(100 * 1024 * 1024), // 100 MB/s
      prev_network_rx_(0),
      prev_network_tx_(0),
      prev_network_time_(std::chrono::system_clock::now()),
      first_check_(true) {
}

void AlertManager::CheckMetrics(const SystemMetrics& metrics) {
    // Check CPU
    if (metrics.cpu_percent > cpu_threshold_) {
        if (!IsAlertActive(AlertType::CPU_HIGH)) {
            TriggerAlert(AlertType::CPU_HIGH,
                        "CPU usage is high",
                        cpu_threshold_,
                        metrics.cpu_percent);
        }
    } else {
        if (IsAlertActive(AlertType::CPU_HIGH)) {
            ResolveAlert(AlertType::CPU_HIGH);
        }
    }
    
    // Check Memory
    if (metrics.memory_percent > memory_threshold_) {
        if (!IsAlertActive(AlertType::MEMORY_HIGH)) {
            TriggerAlert(AlertType::MEMORY_HIGH,
                        "Memory usage is high",
                        memory_threshold_,
                        metrics.memory_percent);
        }
    } else {
        if (IsAlertActive(AlertType::MEMORY_HIGH)) {
            ResolveAlert(AlertType::MEMORY_HIGH);
        }
    }
    
    // Check Disk
    if (metrics.disk_percent > disk_threshold_) {
        if (!IsAlertActive(AlertType::DISK_HIGH)) {
            TriggerAlert(AlertType::DISK_HIGH,
                        "Disk usage is high",
                        disk_threshold_,
                        metrics.disk_percent);
        }
    } else {
        if (IsAlertActive(AlertType::DISK_HIGH)) {
            ResolveAlert(AlertType::DISK_HIGH);
        }
    }
    
    // Check Network (calculate rate)
    if (!first_check_) {
        auto now = std::chrono::system_clock::now();
        auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(now - prev_network_time_).count();
        
        if (time_diff > 0) {
            uint64_t rx_diff = metrics.network_rx_bytes - prev_network_rx_;
            uint64_t tx_diff = metrics.network_tx_bytes - prev_network_tx_;
            uint64_t total_diff = rx_diff + tx_diff;
            uint64_t rate = total_diff / time_diff;
            
            if (rate > network_threshold_) {
                if (!IsAlertActive(AlertType::NETWORK_HIGH)) {
                    TriggerAlert(AlertType::NETWORK_HIGH,
                                "Network traffic is high",
                                network_threshold_,
                                static_cast<double>(rate));
                }
            } else {
                if (IsAlertActive(AlertType::NETWORK_HIGH)) {
                    ResolveAlert(AlertType::NETWORK_HIGH);
                }
            }
        }
        
        prev_network_rx_ = metrics.network_rx_bytes;
        prev_network_tx_ = metrics.network_tx_bytes;
        prev_network_time_ = now;
    } else {
        prev_network_rx_ = metrics.network_rx_bytes;
        prev_network_tx_ = metrics.network_tx_bytes;
        prev_network_time_ = std::chrono::system_clock::now();
        first_check_ = false;
    }
}

void AlertManager::TriggerAlert(AlertType type, const std::string& message,
                                double threshold, double current_value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Alert alert;
    alert.type = type;
    alert.message = message;
    alert.threshold = threshold;
    alert.current_value = current_value;
    alert.timestamp = std::chrono::system_clock::now();
    alert.active = true;
    
    active_alerts_.push_back(alert);
    alert_history_.push_back(alert);
    
    // Keep history limited
    if (alert_history_.size() > 1000) {
        alert_history_.erase(alert_history_.begin());
    }
    
    if (alert_callback_) {
        alert_callback_(alert);
    }
}

void AlertManager::ResolveAlert(AlertType type) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    active_alerts_.erase(
        std::remove_if(active_alerts_.begin(), active_alerts_.end(),
                      [type](const Alert& a) { return a.type == type; }),
        active_alerts_.end()
    );
}

bool AlertManager::IsAlertActive(AlertType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return std::any_of(active_alerts_.begin(), active_alerts_.end(),
                      [type](const Alert& a) { return a.type == type && a.active; });
}

std::vector<Alert> AlertManager::GetActiveAlerts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_alerts_;
}

std::vector<Alert> AlertManager::GetAlertHistory(size_t count) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t start = (alert_history_.size() > count) ? alert_history_.size() - count : 0;
    return std::vector<Alert>(alert_history_.begin() + start, alert_history_.end());
}

void AlertManager::ClearResolvedAlerts() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    active_alerts_.erase(
        std::remove_if(active_alerts_.begin(), active_alerts_.end(),
                      [](const Alert& a) { return !a.active; }),
        active_alerts_.end()
    );
}

std::string Alert::ToJson() const {
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    
    json << "{"
         << "\"type\":\"" << AlertTypeToString(type) << "\","
         << "\"message\":\"" << message << "\","
         << "\"threshold\":" << threshold << ","
         << "\"current_value\":" << current_value << ","
         << "\"timestamp\":" << time_t << ","
         << "\"active\":" << (active ? "true" : "false")
         << "}";
    
    return json.str();
}

} // namespace http

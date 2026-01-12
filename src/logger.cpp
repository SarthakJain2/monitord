#include "logger.hpp"
#include <iomanip>
#include <sstream>

namespace http {

Logger::Logger() : level_(LogLevel::INFO), use_file_(false) {
}

Logger::Logger(const std::string& log_file) : level_(LogLevel::INFO), use_file_(true) {
    log_file_.open(log_file, std::ios::app);
    if (!log_file_.is_open()) {
        use_file_ = false;
    }
}

Logger::~Logger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::Debug(const std::string& message) {
    Log(LogLevel::DEBUG, message);
}

void Logger::Info(const std::string& message) {
    Log(LogLevel::INFO, message);
}

void Logger::Warn(const std::string& message) {
    Log(LogLevel::WARN, message);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::ERROR, message);
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (level < level_) {
        return;
    }
    
    WriteLog(level, message);
}

std::string Logger::LevelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARN: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::WriteLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string log_entry = "[" + GetTimestamp() + "] [" + LevelToString(level) + "] " + message;
    
    if (use_file_ && log_file_.is_open()) {
        log_file_ << log_entry << std::endl;
        log_file_.flush();
    } else {
        std::cout << log_entry << std::endl;
    }
}

} // namespace http

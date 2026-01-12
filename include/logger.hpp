#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace http {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    Logger();
    explicit Logger(const std::string& log_file);
    ~Logger();

    // Non-copyable, non-movable (mutex is not movable)
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) noexcept = delete;
    Logger& operator=(Logger&&) noexcept = delete;

    void SetLevel(LogLevel level) { level_ = level; }
    LogLevel GetLevel() const { return level_; }

    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warn(const std::string& message);
    void Error(const std::string& message);

    void Log(LogLevel level, const std::string& message);

private:
    std::string LevelToString(LogLevel level) const;
    std::string GetTimestamp() const;
    void WriteLog(LogLevel level, const std::string& message);

    LogLevel level_;
    std::ofstream log_file_;
    std::mutex log_mutex_;
    bool use_file_;
};

} // namespace http

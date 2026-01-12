#pragma once

#include <string>
#include <cstdint>

namespace http {

struct Config {
    std::string host = "0.0.0.0";
    uint16_t port = 8080;
    size_t thread_pool_size = 4;
    size_t max_connections = 1000;
    size_t read_buffer_size = 8192;
    size_t request_timeout_seconds = 30;
    std::string log_file = "";
    bool enable_logging = true;
    std::string static_directory = "";

    // Load from environment variables or file
    static Config FromEnv();
    static Config FromFile(const std::string& file_path);
};

} // namespace http

#include "config.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace http {

Config Config::FromEnv() {
    Config config;
    
    const char* host = std::getenv("SERVER_HOST");
    if (host) config.host = host;
    
    const char* port = std::getenv("SERVER_PORT");
    if (port) config.port = static_cast<uint16_t>(std::stoi(port));
    
    const char* threads = std::getenv("THREAD_POOL_SIZE");
    if (threads) config.thread_pool_size = std::stoul(threads);
    
    const char* max_conn = std::getenv("MAX_CONNECTIONS");
    if (max_conn) config.max_connections = std::stoul(max_conn);
    
    const char* log_file = std::getenv("LOG_FILE");
    if (log_file) config.log_file = log_file;
    
    const char* static_dir = std::getenv("STATIC_DIRECTORY");
    if (static_dir) config.static_directory = static_dir;
    
    return config;
}

Config Config::FromFile(const std::string& file_path) {
    Config config;
    std::ifstream file(file_path);
    
    if (!file.is_open()) {
        return config; // Return default config if file doesn't exist
    }
    
    std::string line;
    while (std::getline(file, line)) {
        // Simple key=value parser
        size_t equals_pos = line.find('=');
        if (equals_pos != std::string::npos) {
            std::string key = line.substr(0, equals_pos);
            std::string value = line.substr(equals_pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            if (key == "host") config.host = value;
            else if (key == "port") config.port = static_cast<uint16_t>(std::stoi(value));
            else if (key == "thread_pool_size") config.thread_pool_size = std::stoul(value);
            else if (key == "max_connections") config.max_connections = std::stoul(value);
            else if (key == "log_file") config.log_file = value;
            else if (key == "static_directory") config.static_directory = value;
        }
    }
    
    return config;
}

} // namespace http

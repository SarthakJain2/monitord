#include "server.hpp"
#include "connection.hpp"
#include "http_parser.hpp"
#include "http_response.hpp"
#include "websocket.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdexcept>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <errno.h>
#include <cstring>
#include <vector>
#include <functional>

namespace http {

Server::Server(const Config& config)
    : config_(config),
      event_loop_(std::make_unique<EventLoop>()),
      thread_pool_(std::make_unique<ThreadPool>(config.thread_pool_size)),
      logger_(config.log_file.empty() ? Logger{} : Logger{config.log_file}),
      running_(false),
      server_fd_(-1) {
    
    if (config.enable_logging) {
        logger_.SetLevel(LogLevel::INFO);
    } else {
        logger_.SetLevel(LogLevel::ERROR);
    }
}

Server::~Server() {
    Stop();
}

void Server::Get(const std::string& path, RouteHandler handler) {
    router_.Register(HttpMethod::GET, path, std::move(handler));
}

void Server::Post(const std::string& path, RouteHandler handler) {
    router_.Register(HttpMethod::POST, path, std::move(handler));
}

void Server::Put(const std::string& path, RouteHandler handler) {
    router_.Register(HttpMethod::PUT, path, std::move(handler));
}

void Server::Delete(const std::string& path, RouteHandler handler) {
    router_.Register(HttpMethod::DELETE, path, std::move(handler));
}

void Server::Patch(const std::string& path, RouteHandler handler) {
    router_.Register(HttpMethod::PATCH, path, std::move(handler));
}

void Server::ServeStatic(const std::string& path, const std::string& directory) {
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        logger_.Warn("Static directory does not exist: " + directory);
        return;
    }
    
    Get(path + "/*", [directory, path](const HttpRequest& request) -> HttpResponse {
        std::string request_path = request.path;
        
        // Remove the route prefix
        if (request_path.find(path) == 0) {
            request_path = request_path.substr(path.length());
        }
        
        // Remove leading slash
        if (!request_path.empty() && request_path[0] == '/') {
            request_path = request_path.substr(1);
        }
        
        // Build full file path
        std::filesystem::path file_path = std::filesystem::path(directory) / request_path;
        
        // Security: prevent directory traversal
        std::filesystem::path canonical_dir = std::filesystem::canonical(directory);
        std::filesystem::path canonical_file;
        
        try {
            canonical_file = std::filesystem::canonical(file_path);
        } catch (...) {
            return NotFound("File not found");
        }
        
        // Check if file is within the static directory
        std::string dir_str = canonical_dir.string();
        std::string file_str = canonical_file.string();
        
        if (file_str.find(dir_str) != 0) {
            return Forbidden("Access denied");
        }
        
        return HttpResponse::FromFile(canonical_file.string());
    });
}

void Server::Start() {
    // Create socket
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to set socket options");
    }
    
    // Set non-blocking
    int flags = fcntl(server_fd_, F_GETFL, 0);
    fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);
    
    // Bind socket
    struct sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(config_.host.c_str());
    address.sin_port = htons(config_.port);
    
    if (bind(server_fd_, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to bind socket");
    }
    
    // Listen
    if (listen(server_fd_, static_cast<int>(config_.max_connections)) < 0) {
        close(server_fd_);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    running_ = true;
    logger_.Info("Server starting on " + config_.host + ":" + std::to_string(config_.port));
    
    // Register server socket for incoming connections
    event_loop_->RegisterRead(server_fd_, [this](int fd, EventType type) {
        if (type == EventType::Read) {
            // Accept multiple connections in a loop
            while (true) {
                struct sockaddr_in client_addr{};
                socklen_t client_addr_len = sizeof(client_addr);
                
                int client_fd = accept(fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_addr_len);
                if (client_fd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // No more connections to accept
                        break;
                    }
                    // Error accepting, but continue
                    break;
                }
                
                // Set non-blocking
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                
                // Register client connection for reading (only once)
                event_loop_->RegisterRead(client_fd, [this](int client_fd, EventType /*type*/) {
                    // Unregister immediately to prevent multiple triggers
                    event_loop_->Unregister(client_fd);
                    HandleConnection(client_fd);
                });
            }
        }
    });
    
    // Start event loop in a separate thread
    std::thread event_thread([this]() {
        event_loop_->Run();
    });
    
    event_thread.detach();
    
    // Wait for stop signal
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void Server::Stop() {
    if (!running_) return;
    
    running_ = false;
    event_loop_->Stop();
    
    if (server_fd_ >= 0) {
        close(server_fd_);
        server_fd_ = -1;
    }
    
    logger_.Info("Server stopped");
}

void Server::HandleConnection(int client_fd) {
    // Check if file descriptor is still valid
    if (fcntl(client_fd, F_GETFL) < 0) {
        // File descriptor is invalid, don't process
        return;
    }
    
    thread_pool_->Enqueue([this, client_fd]() {
        try {
            // Check again in the thread
            if (fcntl(client_fd, F_GETFL) < 0) {
                return; // FD closed
            }
            
            std::string request_data = ReadRequest(client_fd);
            
            if (!request_data.empty()) {
                // Check if this is a WebSocket upgrade request
                bool is_websocket = WebSocket::IsWebSocketRequest(request_data);
                
                // Check FD is still valid before processing
                if (fcntl(client_fd, F_GETFL) >= 0) {
                    ProcessRequest(client_fd, request_data);
                    
                    // If it's a WebSocket connection, don't close it - the handler will manage it
                    if (is_websocket && websocket_connections_.count(client_fd) > 0) {
                        // WebSocket connection - handler manages lifecycle
                        return;
                    }
                }
            }
            
            // Close connection (only if not WebSocket)
            if (fcntl(client_fd, F_GETFL) >= 0 && websocket_connections_.count(client_fd) == 0) {
                close(client_fd);
            }
        } catch (const std::exception& e) {
            logger_.Error("Exception in HandleConnection: " + std::string(e.what()));
            if (fcntl(client_fd, F_GETFL) >= 0 && websocket_connections_.count(client_fd) == 0) {
                close(client_fd);
            }
        } catch (...) {
            logger_.Error("Unknown exception in HandleConnection");
            if (fcntl(client_fd, F_GETFL) >= 0 && websocket_connections_.count(client_fd) == 0) {
                close(client_fd);
            }
        }
    });
}

std::string Server::ReadRequest(int client_fd) {
    std::string request;
    std::vector<char> buffer(config_.read_buffer_size);
    
    // For non-blocking sockets, we need to read in a loop with small delays
    int attempts = 0;
    const int max_attempts = 100; // Max 1 second of trying
    
    while (attempts < max_attempts) {
        ssize_t bytes_read = read(client_fd, buffer.data(), buffer.size() - 1);
        
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available right now, wait a bit
                attempts++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else {
                // Real error
                throw std::runtime_error("Error reading from socket: " + std::string(strerror(errno)));
            }
        } else if (bytes_read == 0) {
            // Connection closed
            if (request.empty()) {
                // Connection closed before any data
                return "";
            }
            break;
        } else {
            // Got data
            buffer[bytes_read] = '\0';
            request.append(buffer.data(), bytes_read);
            attempts = 0; // Reset attempts on successful read
            
            // Check if we've received the full request
            if (request.find("\r\n\r\n") != std::string::npos) {
                // Check for Content-Length header
                size_t content_length_pos = request.find("Content-Length:");
                if (content_length_pos != std::string::npos) {
                    size_t header_end = request.find("\r\n", content_length_pos);
                    if (header_end != std::string::npos) {
                        std::string content_length_str = request.substr(
                            content_length_pos + 15,
                            header_end - content_length_pos - 15
                        );
                        
                        // Trim whitespace
                        content_length_str.erase(0, content_length_str.find_first_not_of(" \t"));
                        content_length_str.erase(content_length_str.find_last_not_of(" \t") + 1);
                        
                        if (!content_length_str.empty()) {
                            try {
                                size_t content_length = std::stoul(content_length_str);
                                size_t body_start = request.find("\r\n\r\n") + 4;
                                size_t body_received = request.length() - body_start;
                                
                                if (body_received < content_length) {
                                    // Need to read more
                                    continue;
                                }
                            } catch (...) {
                                // Invalid content-length, assume we have everything
                            }
                        }
                    }
                }
                // We have the full request
                break;
            }
        }
    }
    
    if (attempts >= max_attempts && request.empty()) {
        // Timeout waiting for data
        return "";
    }
    
    return request;
}

void Server::ProcessRequest(int client_fd, const std::string& request_data) {
    try {
        // Check for WebSocket upgrade
        if (WebSocket::IsWebSocketRequest(request_data)) {
            HandleWebSocket(client_fd, request_data);
            return;
        }
        
        HttpRequest request = HttpParser::Parse(request_data);
        
        Connection conn(client_fd);
        logger_.Info("[" + conn.GetRemoteAddress() + "] " +
                     HttpParser::MethodToString(request.method) + " " + request.path);
        
        HttpResponse response = router_.HandleRequest(request);
        
        SendResponse(client_fd, response);
    } catch (const std::exception& e) {
        logger_.Error("Error processing request: " + std::string(e.what()));
        HttpResponse error_response = InternalError("Internal Server Error");
        SendResponse(client_fd, error_response);
    }
}

void Server::SendResponse(int client_fd, const HttpResponse& response) {
    std::string response_str = response.ToString();
    ssize_t bytes_sent = 0;
    size_t total_bytes = response_str.length();
    
    // Try to send all data, but don't block forever
    int attempts = 0;
    const int max_attempts = 100;
    
    while (bytes_sent < static_cast<ssize_t>(total_bytes) && attempts < max_attempts) {
        ssize_t sent = write(client_fd, response_str.c_str() + bytes_sent, total_bytes - bytes_sent);
        
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                attempts++;
                continue;
            } else {
                logger_.Error("Error writing to socket: " + std::string(strerror(errno)));
                break;
            }
        } else if (sent == 0) {
            // Connection closed
            logger_.Warn("Connection closed while writing response");
            break;
        }
        
        bytes_sent += sent;
        attempts = 0; // Reset attempts on successful write
    }
    
    if (bytes_sent < static_cast<ssize_t>(total_bytes)) {
        logger_.Warn("Failed to send complete response: " + std::to_string(bytes_sent) + "/" + std::to_string(total_bytes));
    }
}

void Server::HandleWebSocket(int client_fd, const std::string& request) {
    HttpRequest parsed = HttpParser::Parse(request);
    std::string path = parsed.path;
    
    // Send handshake response
    std::string handshake = WebSocket::GenerateHandshakeResponse(request);
    if (handshake.empty()) {
        close(client_fd);
        return;
    }
    
    // Send handshake directly (not through SendResponse)
    send(client_fd, handshake.c_str(), handshake.length(), 0);
    
    // Mark as WebSocket connection
    websocket_connections_[client_fd] = true;
    
    // Find and call handler
    auto it = websocket_handlers_.find(path);
    if (it != websocket_handlers_.end()) {
        it->second(client_fd, request);
    } else {
        // Default: keep connection open but do nothing
        close(client_fd);
        websocket_connections_.erase(client_fd);
    }
}

void Server::RegisterWebSocketHandler(const std::string& path, std::function<void(int, const std::string&)> handler) {
    websocket_handlers_[path] = handler;
}

} // namespace http

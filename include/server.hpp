#pragma once

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "event_loop.hpp"
#include "thread_pool.hpp"
#include "router.hpp"
#include "logger.hpp"
#include "config.hpp"
#include "websocket.hpp"
#include <unordered_map>
#include <atomic>

namespace http {

class Server {
public:
    explicit Server(const Config& config = Config{});
    ~Server();

    // Non-copyable, non-movable (Logger is not movable)
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    Server(Server&&) noexcept = delete;
    Server& operator=(Server&&) noexcept = delete;

    // Route registration
    void Get(const std::string& path, RouteHandler handler);
    void Post(const std::string& path, RouteHandler handler);
    void Put(const std::string& path, RouteHandler handler);
    void Delete(const std::string& path, RouteHandler handler);
    void Patch(const std::string& path, RouteHandler handler);

    // Static file serving
    void ServeStatic(const std::string& path, const std::string& directory);

    // Start the server
    void Start();
    void Stop();

    // Check if server is running
    bool IsRunning() const { return running_; }

    // WebSocket support
    void HandleWebSocket(int client_fd, const std::string& request);
    void RegisterWebSocketHandler(const std::string& path, std::function<void(int, const std::string&)> handler);

private:
    // WebSocket connections
    std::unordered_map<int, bool> websocket_connections_;
    std::unordered_map<std::string, std::function<void(int, const std::string&)>> websocket_handlers_;
    void HandleConnection(int client_fd);
    void ProcessRequest(int client_fd, const std::string& request_data);
    std::string ReadRequest(int client_fd);
    void SendResponse(int client_fd, const HttpResponse& response);

    Config config_;
    std::unique_ptr<EventLoop> event_loop_;
    std::unique_ptr<ThreadPool> thread_pool_;
    Router router_;
    Logger logger_;
    bool running_;
    int server_fd_;
};

} // namespace http

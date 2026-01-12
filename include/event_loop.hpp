#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <sys/event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

namespace http {

enum class EventType {
    Read,
    Write,
    Error
};

using EventCallback = std::function<void(int fd, EventType type)>;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Non-copyable, movable
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop(EventLoop&&) noexcept;
    EventLoop& operator=(EventLoop&&) noexcept;

    // Register file descriptor for events
    void RegisterRead(int fd, EventCallback callback);
    void RegisterWrite(int fd, EventCallback callback);
    void Unregister(int fd);

    // Run the event loop
    void Run();
    void Stop();

    bool IsRunning() const { return running_; }

private:
    void ProcessEvents();
    int kqueue_fd_;
    bool running_;
    std::unordered_map<int, EventCallback> read_callbacks_;
    std::unordered_map<int, EventCallback> write_callbacks_;
};

} // namespace http

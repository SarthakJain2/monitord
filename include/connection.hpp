#pragma once

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>

namespace http {

class Connection {
public:
    explicit Connection(int fd);
    ~Connection();

    // Non-copyable, movable
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;

    int GetFd() const { return fd_; }
    std::string GetRemoteAddress() const;
    uint16_t GetRemotePort() const;

    ssize_t Read(char* buffer, size_t size);
    ssize_t Write(const char* data, size_t size);
    ssize_t Write(const std::string& data);

    void Close();
    bool IsOpen() const { return fd_ >= 0; }

    void SetNonBlocking(bool non_blocking);
    void SetKeepAlive(bool keep_alive);

private:
    int fd_;
    struct sockaddr_in addr_;
    bool addr_initialized_;
};

} // namespace http

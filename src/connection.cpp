#include "connection.hpp"
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>

namespace http {

Connection::Connection(int fd) : fd_(fd), addr_initialized_(false) {
    if (fd_ < 0) {
        throw std::invalid_argument("Invalid file descriptor");
    }
    
    socklen_t addr_len = sizeof(addr_);
    if (getpeername(fd_, reinterpret_cast<struct sockaddr*>(&addr_), &addr_len) == 0) {
        addr_initialized_ = true;
    }
}

Connection::~Connection() {
    Close();
}

Connection::Connection(Connection&& other) noexcept
    : fd_(other.fd_),
      addr_(other.addr_),
      addr_initialized_(other.addr_initialized_) {
    other.fd_ = -1;
    other.addr_initialized_ = false;
}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        Close();
        fd_ = other.fd_;
        addr_ = other.addr_;
        addr_initialized_ = other.addr_initialized_;
        other.fd_ = -1;
        other.addr_initialized_ = false;
    }
    return *this;
}

std::string Connection::GetRemoteAddress() const {
    if (!addr_initialized_) {
        return "unknown";
    }
    return inet_ntoa(addr_.sin_addr);
}

uint16_t Connection::GetRemotePort() const {
    if (!addr_initialized_) {
        return 0;
    }
    return ntohs(addr_.sin_port);
}

ssize_t Connection::Read(char* buffer, size_t size) {
    return ::read(fd_, buffer, size);
}

ssize_t Connection::Write(const char* data, size_t size) {
    return ::write(fd_, data, size);
}

ssize_t Connection::Write(const std::string& data) {
    return Write(data.c_str(), data.length());
}

void Connection::Close() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

void Connection::SetNonBlocking(bool non_blocking) {
    int flags = fcntl(fd_, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("Failed to get socket flags");
    }
    
    if (non_blocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    if (fcntl(fd_, F_SETFL, flags) == -1) {
        throw std::runtime_error("Failed to set socket flags");
    }
}

void Connection::SetKeepAlive(bool keep_alive) {
    int optval = keep_alive ? 1 : 0;
    if (setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
        throw std::runtime_error("Failed to set keep-alive");
    }
}

} // namespace http

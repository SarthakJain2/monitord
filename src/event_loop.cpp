#include "event_loop.hpp"
#include <stdexcept>
#include <iostream>
#include <unordered_map>
#include <errno.h>

namespace http {

EventLoop::EventLoop() : running_(false) {
    kqueue_fd_ = kqueue();
    if (kqueue_fd_ == -1) {
        throw std::runtime_error("Failed to create kqueue");
    }
}

EventLoop::~EventLoop() {
    if (kqueue_fd_ >= 0) {
        close(kqueue_fd_);
    }
}

EventLoop::EventLoop(EventLoop&& other) noexcept
    : kqueue_fd_(other.kqueue_fd_),
      running_(other.running_),
      read_callbacks_(std::move(other.read_callbacks_)),
      write_callbacks_(std::move(other.write_callbacks_)) {
    other.kqueue_fd_ = -1;
    other.running_ = false;
}

EventLoop& EventLoop::operator=(EventLoop&& other) noexcept {
    if (this != &other) {
        if (kqueue_fd_ >= 0) {
            close(kqueue_fd_);
        }
        kqueue_fd_ = other.kqueue_fd_;
        running_ = other.running_;
        read_callbacks_ = std::move(other.read_callbacks_);
        write_callbacks_ = std::move(other.write_callbacks_);
        other.kqueue_fd_ = -1;
        other.running_ = false;
    }
    return *this;
}

void EventLoop::RegisterRead(int fd, EventCallback callback) {
    struct kevent change_event;
    EV_SET(&change_event, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    
    if (kevent(kqueue_fd_, &change_event, 1, nullptr, 0, nullptr) == -1) {
        throw std::runtime_error("Failed to register read event");
    }
    
    read_callbacks_[fd] = std::move(callback);
}

void EventLoop::RegisterWrite(int fd, EventCallback callback) {
    struct kevent change_event;
    EV_SET(&change_event, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    
    if (kevent(kqueue_fd_, &change_event, 1, nullptr, 0, nullptr) == -1) {
        throw std::runtime_error("Failed to register write event");
    }
    
    write_callbacks_[fd] = std::move(callback);
}

void EventLoop::Unregister(int fd) {
    struct kevent change_event;
    EV_SET(&change_event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(kqueue_fd_, &change_event, 1, nullptr, 0, nullptr);
    
    EV_SET(&change_event, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    kevent(kqueue_fd_, &change_event, 1, nullptr, 0, nullptr);
    
    read_callbacks_.erase(fd);
    write_callbacks_.erase(fd);
}

void EventLoop::Run() {
    running_ = true;
    while (running_) {
        ProcessEvents();
    }
}

void EventLoop::Stop() {
    running_ = false;
}

void EventLoop::ProcessEvents() {
    struct kevent events[64];
    int num_events = kevent(kqueue_fd_, nullptr, 0, events, 64, nullptr);
    
    if (num_events == -1) {
        if (errno != EINTR) {
            throw std::runtime_error("kevent failed");
        }
        return;
    }
    
    for (int i = 0; i < num_events; ++i) {
        int fd = static_cast<int>(events[i].ident);
        
        if (events[i].filter == EVFILT_READ) {
            auto it = read_callbacks_.find(fd);
            if (it != read_callbacks_.end()) {
                it->second(fd, EventType::Read);
            }
        } else if (events[i].filter == EVFILT_WRITE) {
            auto it = write_callbacks_.find(fd);
            if (it != write_callbacks_.end()) {
                it->second(fd, EventType::Write);
            }
        }
        
        if (events[i].flags & EV_EOF) {
            Unregister(fd);
        }
    }
}

} // namespace http

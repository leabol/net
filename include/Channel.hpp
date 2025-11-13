#pragma once
#include <functional>
#include <sys/epoll.h>

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(int fd) : fd_(fd) {}

    void setReadCallback(EventCallback func) { readCallback_ = std::move(func); }
    void setWriteCallback(EventCallback func) { writeCallback_ = std::move(func); }

    void setInterestedEvents(uint32_t nevents) { events_ = nevents; }
    uint32_t getInterestedEvents() const { return events_; }

    void setReadyEvents(uint32_t revents) { revents_ = revents; }
    void handleEvent() {
        if ((revents_ & EPOLLIN) && readCallback_) {
            readCallback_();
        }
        if ((revents_ & EPOLLOUT) && writeCallback_) {
            writeCallback_();
        }
        //add more error msg
    }

    int getFd() const { return fd_; }

private:
    int fd_ = -1;
    uint32_t events_ = 0;
    uint32_t revents_ = 0;
    EventCallback readCallback_;
    EventCallback writeCallback_;
};
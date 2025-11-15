#pragma once
#include <sys/epoll.h>

#include <functional>

namespace Server {
class EventLoop;

class Channel {
  public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd) : loop_(loop), fd_(fd) {}

    void setReadCallback(EventCallback func) {
        readCallback_ = std::move(func);
    }
    void setWriteCallback(EventCallback func) {
        writeCallback_ = std::move(func);
    }
    void setCloseCallback(EventCallback func) {
        closeCallback_ = std::move(func);
    }
    void enableReading() {
        events_ |= EPOLLIN;
        update();
    }
    void disableReading() {
        events_ &= ~EPOLLIN;
        update();
    }
    void enableWriting() {
        events_ |= EPOLLOUT;
        update();
    }
    void disableWriting() {
        events_ &= ~EPOLLOUT;
        update();
    }
    void disableAll() {
        events_ = 0;  // 清空所有事件
        update();
    }
    [[nodiscard]] bool isWriting() const {
        return (events_ & EPOLLOUT) != 0;
    }
    [[nodiscard]] bool isReading() const {
        return (events_ & EPOLLIN) != 0;
    }

    [[nodiscard]] uint32_t getInterestedEvents() const {
        return events_;
    }

    void setReadyEvents(uint32_t revents) {
        revents_ = revents;
    }

    [[nodiscard]] int getFd() const {
        return fd_;
    }

    void update();

    void handleEvent();

    void remove();

    // 由 Poller 管理：记录是否已注册到 epoll（避免重复 ADD）
    void setAdded(bool v) {
        added_ = v;
    }
    [[nodiscard]] bool isAdded() const {
        return added_;
    }

  private:
    EventLoop*    loop_;
    int           fd_{-1};
    uint32_t      events_{0};
    uint32_t      revents_{0};
    EventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    bool          added_{false};
};
}  // namespace Server

#include "../include/EventLoop.hpp"

#include "../include/Channel.hpp"
#include "../include/EpollPoller.hpp"

using namespace Server;

EventLoop::EventLoop() : poller_(std::make_unique<EpollPoller>()) {}

EventLoop::~EventLoop() = default;

void EventLoop::loop() {
    while (true) {
        std::vector<Channel*> activeChannels = poller_->poll(1000);
        for (auto channel : activeChannels) {
            channel->handleEvent();
        }
    }
}

void EventLoop::addChannel(std::shared_ptr<Channel> channel) {
    if (!channel) {
        return;
    }
    int fd        = channel->getFd();
    channels_[fd] = std::move(channel);
    poller_->addChannel(channels_[fd].get());
}

void EventLoop::updateChannel(Channel* channel) {
    if (!channel) {
        return;
    }
    poller_->addChannel(channel);
}

void EventLoop::removeChannel(int fd) {
    auto it = channels_.find(fd);
    if (it == channels_.end()) {
        return;
    }
    poller_->removeChannel(it->second.get());
    channels_.erase(it);
}

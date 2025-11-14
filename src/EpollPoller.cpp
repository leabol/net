#include "../include/EpollPoller.hpp"

#include <cerrno>
#include <cstring>

#include "../include/Channel.hpp"

using namespace Server;

EpollPoller::EpollPoller() : epollfd_(epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize) {
    if (epollfd_ < 0) {
        // handle error
    }
}

EpollPoller::~EpollPoller() {
    close(epollfd_);
}

std::vector<Channel*> EpollPoller::poll(int timeout) {
    int numEvents = epoll_wait(epollfd_, events_.data(), static_cast<int>(events_.size()), timeout);

    if (numEvents < 0) {
        if (errno == EINTR) {
            return {};
        }
        // TODO: add logging for unexpected epoll_wait failure.
        return {};
    }

    std::vector<Channel*> activeChannel;
    activeChannel.reserve(static_cast<size_t>(numEvents));
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setReadyEvents(events_[i].events);
        activeChannel.push_back(channel);
    }

    return activeChannel;
}

void EpollPoller::addChannel(Channel* channel) {
    epoll_event event;
    event.data.ptr = channel;
    event.events   = channel->getInterestedEvents();

    int fd = channel->getFd();
    if (channels_.find(fd) == channels_.end()) {
        channels_[fd] = channel;
        epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event);
    } else {
        epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event);
    }
}

void EpollPoller::removeChannel(Channel* channel) {
    if (!channel) {
        return;
    }
    int fd = channel->getFd();
    epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, nullptr);
    channels_.erase(fd);
}

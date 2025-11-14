#pragma once
#include <sys/epoll.h>
#include <unistd.h>

#include <unordered_map>
#include <vector>

namespace Server {

class Channel;
class EpollPoller {
  public:
    EpollPoller();
    ~EpollPoller();

    // 返回活跃的Channel
    std::vector<Channel*> poll(int timeout);

    void addChannel(Channel* channel);
    void removeChannel(Channel* channel);

  private:
    static const int                  kInitEventListSize = 16;
    int                               epollfd_           = -1;
    std::vector<epoll_event>          events_;
    std::unordered_map<int, Channel*> channels_;
};
}  // namespace Server

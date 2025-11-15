#pragma once
#include <memory>
#include <unordered_map>

namespace Server {

class Channel;
class EpollPoller;

// 对epoll的分装, 对外提供更多的接口
class EventLoop {
  public:
    EventLoop();
    ~EventLoop();

    void loop(int timeout);

    void addChannel(std::shared_ptr<Channel> channel);  // 移交channel的所有权给eventloop
    void updateChannel(Channel* channel);  // 只能有channel类中调用, 外部不可直接使用
    void removeChannel(Channel* channel);  // 只能有channel类中调用, 外部不可直接使用

  private:
    std::unique_ptr<EpollPoller>                      poller_;
    std::unordered_map<int, std::shared_ptr<Channel>> channels_;
};
}  // namespace Server

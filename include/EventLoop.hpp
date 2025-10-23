#include <memory>
#include <unordered_map>
#include <vector>

class Channel;
class EpollPoller;

class EventLoop {
public:
    EventLoop();
    ~EventLoop();
    void loop();
    void addChannel(std::shared_ptr<Channel> channel);
    void updateChannel(Channel *channel);
    void removeChannel(int fd);
private:
    std::unique_ptr<EpollPoller> poller_;
    std::unordered_map<int, std::shared_ptr<Channel>> channels_;
};


#include <unistd.h>

#include "../include/Acceptor.hpp"
#include "../include/Channel.hpp"
#include "../include/EventLoop.hpp"
#include "../include/InetAddress.hpp"
#include "../include/Log.hpp"
#include "../include/Socket.hpp"

using namespace Server;

int main() {
    initLogger();

    // 构造一些核心对象，确保编译/链接路径齐备
    Socket s;         // 创建 fd
    s.setNonblock();  // 设置非阻塞

    // 解析一个本地端口（0 表示内核分配），不实际 listen
    InetAddress any{"0"};

    EventLoop loop;  // 不启动 loop，仅构造

    // 构造 Channel 但不注册到 epoll（不调用 enableReading）
    Channel ch{&loop, s.fd()};

    // Acceptor 构造（会 bind，但是不调用 listen）
    // 为避免真实端口占用，这里单独 new 一个 Socket 进行快速绑定到 0 端口
    Acceptor acc{&loop, any};

    LOG_INFO("core_build_test constructed core objects successfully");
    return 0;
}

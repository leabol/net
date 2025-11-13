#include "../include/Socket.hpp"
#include "../include/InetAddress.hpp"
#include "../include/Log.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <unistd.h>

using namespace Server;

static void set_blocking(int fd, bool blocking) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags == -1) return;
    if (blocking) flags &= ~O_NONBLOCK; else flags |= O_NONBLOCK;
    ::fcntl(fd, F_SETFL, flags);
}

int main(int argc, char** argv) {
    Server::initLogger();

    std::string port = (argc >= 2) ? argv[1] : std::string{"8091"};

    try {
        Socket listenSock;
        listenSock.setReuseAddr(true);
        // 可选：按需开启 reuseport
        // listenSock.setReusePort(true);

        // InetAddress addr(port);
        // listenSock.bindAddr(addr);
        listenSock.bindAddr(port);
        listenSock.listen(128);
        LOG_INFO("socket_server_demo listening on port {}", port);

        while (true) {
            InetAddress peer("0");
            int connfd = listenSock.accept(peer);
            LOG_INFO("accepted one connection, fd={}", connfd);

            // 为了简单演示，切换为阻塞 I/O，便于 read()/write() 使用
            set_blocking(connfd, true);

            char buf[1024];
            while (true) {
                ssize_t n = ::read(connfd, buf, sizeof(buf));
                if (n > 0) {
                    std::string_view sv(buf, static_cast<size_t>(n));
                    LOG_INFO("recv {} bytes: {}", n, std::string(sv));
                    ssize_t m = ::write(connfd, buf, static_cast<size_t>(n));
                    if (m < 0) {
                        LOG_ERROR("write failed: {}", std::strerror(errno));
                        break;
                    }
                } else if (n == 0) {
                    LOG_INFO("peer closed");
                    break;
                } else {
                    if (errno == EINTR) continue;
                    LOG_ERROR("read failed: {}", std::strerror(errno));
                    break;
                }
            }
            ::close(connfd);
        }
    } catch (const std::exception& ex) {
        LOG_CRITICAL("server fatal: {}", ex.what());
        return 1;
    }
    return 0;
}

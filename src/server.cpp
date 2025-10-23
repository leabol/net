#include "../include/socket.hpp"
#include "../include/address.hpp"
#include "../include/EventLoop.hpp"
#include "../include/Channel.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <unistd.h>

namespace {

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("failed to get file status flags: " + std::string(strerror(errno)));
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("failed to set non-blocking mode: " + std::string(strerror(errno)));
    }
}

void closeConnection(EventLoop &loop, int fd) {
    loop.removeChannel(fd);
    close(fd);
}

void handleNewConnection(EventLoop &loop, int listenfd) {
    while (true) {
        sockaddr_in client{};
        socklen_t len = sizeof(client);
        int connfd = accept4(listenfd, reinterpret_cast<sockaddr*>(&client), &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (connfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            }
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "accept4 failed: " << strerror(errno) << std::endl;
            return;
        }

        auto connChannel = std::make_shared<Channel>(connfd);
        connChannel->setInterestedEvents(EPOLLIN);
        connChannel->setReadCallback([connfd, &loop]() {
            char buf[1024] = {0};
            while (true) {
                ssize_t n = read(connfd, buf, sizeof(buf));
                if (n > 0) {
                    write(1, buf, n);
                    std::cout << std::endl;
                    ssize_t total = 0;
                    while (total < n) {
                        ssize_t sent = write(connfd, buf + total, static_cast<size_t>(n - total));
                        if (sent == -1) {
                            if (errno == EINTR) {
                                continue;
                            }
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                return;
                            }
                            std::cerr << "write failed: " << strerror(errno) << std::endl;
                            closeConnection(loop, connfd);
                            return;
                        }
                        total += sent;
                    }
                } else if (n == 0) {
                    closeConnection(loop, connfd);
                    return;
                } else {
                    if (errno == EINTR) {
                        continue;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }
                    std::cerr << "read failed: " << strerror(errno) << std::endl;
                    closeConnection(loop, connfd);
                    return;
                }
            }
        });

        loop.addChannel(std::move(connChannel));
    }
}

} // namespace

int main() {
    SocketAddr addr = SocketAddr::for_server("8090");

    TcpSocket sockfd;
    sockfd.bindTo(addr);
    sockfd.startListening();

    int listenfd = sockfd.getFd();
    setNonBlocking(listenfd);

    std::cout << "Server listening on port 8090..." << std::endl;

    EventLoop loop;

    auto listenChannel = std::make_shared<Channel>(listenfd);
    listenChannel->setInterestedEvents(EPOLLIN);
    listenChannel->setReadCallback([&loop, listenfd]() {
        handleNewConnection(loop, listenfd);
    });

    loop.addChannel(std::move(listenChannel));
    loop.loop();
}
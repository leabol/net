#include "../include/ServerSocket.hpp"
#include "../include/TcpStream.hpp"
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

void closeConnection(EventLoop &loop, TcpStream& conn) {
    int fd = conn.getFd();
    
    // Release fd ownership FIRST to prevent double close when Channel is destroyed
    conn.release();
    
    // Remove from epoll (this will destroy Channel and any lambda capturing conn)
    loop.removeChannel(fd);
    
    // Close fd last
    if (close(fd) == -1) {
        std::cerr << "[server] close fd " << fd << " failed: " << strerror(errno) << std::endl;
    }
}

void handleConnection(EventLoop &loop, TcpStream& conn) {
    char buf[1024];
    
    // ET mode: must read until EAGAIN, otherwise we miss data
    while (true) {
        ssize_t n = conn.recvRaw(buf, sizeof(buf));
        if (n > 0) {
            write(1, buf, n);
            std::cout << std::endl;
            
            // Echo back
            std::string response(buf, n);
            if (!conn.sendAll(response)) {
                // sendAll returned false: either real error or buffer full
                // In ET mode, we just log and continue, epoll will notify when writable
                std::cerr << "[server] send failed or would block, data may be lost\n";
                closeConnection(loop, conn);
                return;
            }
            // Successfully sent, continue reading
        } else if (n == 0) {
            // Peer closed (EOF)
            closeConnection(loop, conn);
            return;
        } else {
            // n == -1 with real error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data (ET: read all before exiting)
                return;
            }
            std::cerr << "[server] read failed: " << strerror(errno) << std::endl;
            closeConnection(loop, conn);
            return;
        }
    }
}

void handleNewConnection(EventLoop &loop, ServerSocket& listenSock) {
    // Keep accepting connections until no more available (ET mode)
    while (true) {
        try {
            auto conn = std::make_shared<TcpStream>(listenSock.acceptConnection());
            int connfd = conn->getFd();
            
            std::cout << "[server] new connection fd=" << connfd << std::endl;
            
            auto connChannel = std::make_shared<Channel>(connfd);
            connChannel->setInterestedEvents(EPOLLIN | EPOLLET);  // ET mode for data socket
            connChannel->setReadCallback([conn, &loop]() {
                handleConnection(loop, *conn);
            });

            loop.addChannel(std::move(connChannel));
        } catch (const std::exception& ex) {
            // No more connections available or error
            if (std::string(ex.what()).find("no pending connection") != std::string::npos) {
                // Normal: no more pending connections in ET mode
                return;
            }
            // Real error
            std::cerr << "[server] accept failed: " << ex.what() << std::endl;
            return;
        }
    }
}

} // namespace

int main() {
    SocketAddr addr = SocketAddr::for_server("8090");

    ServerSocket sockfd;
    sockfd.bindTo(addr);
    sockfd.startListening();

    int listenfd = sockfd.getFd();

    std::cout << "[server] listening on port 8090...\n";

    EventLoop loop;

    // Create the listen channel
    auto listenChannel = std::make_shared<Channel>(listenfd);
    listenChannel->setInterestedEvents(EPOLLIN);  // LT mode for listen socket
    listenChannel->setReadCallback([&loop, &sockfd]() {
        handleNewConnection(loop, sockfd);
    });
    
    loop.addChannel(std::move(listenChannel));
    loop.loop();
    
    return 0;
}
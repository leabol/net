#include <string>

#include "../include/EventLoop.hpp"
#include "../include/InetAddress.hpp"
#include "../include/Log.hpp"
#include "../include/TcpServer.hpp"

using namespace Server;

int main(int argc, char** argv) {
    Server::initLogger();

    const std::string port = (argc > 1) ? argv[1] : std::string{"9100"};

    EventLoop   loop;
    InetAddress listenAddr(port);
    TcpServer   server(&loop, listenAddr);

    server.setConnectionCallback([](const TcpServer::TcpConnectionPtr& conn) {
        LOG_INFO("connection state change, fd={}", conn->fd());
    });

    server.setMessageCallback([](const TcpServer::TcpConnectionPtr& conn, std::string& message) {
        LOG_INFO("recv from fd={} ({} bytes): {}", conn->fd(), message.size(), message);
        conn->send(std::string{"echo: "} + message);
    });

    server.setWriteCompleteCallback([](const TcpServer::TcpConnectionPtr& conn) {
        LOG_DEBUG("send buffer drained for fd={}", conn->fd());
    });

    server.start();
    LOG_INFO("tcp_server_demo listening on port {} (Ctrl+C to stop)", port);

    loop.loop(1000);
    return 0;
}

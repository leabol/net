#pragma once

#include "SocketBase.hpp"

class SocketAddr;
class TcpStream;

/**
 * @brief Server-side socket for listening and accepting connections
 * 
 * Used to bind to a port and accept incoming TCP connections.
 * Non-blocking mode for ET-triggered epoll operations.
 */
class ServerSocket : public SocketBase {
public:
    ServerSocket() : SocketBase(true) {}  // 服务端用非阻塞（ET 模式需要）

    void bindTo(const SocketAddr& addr);

    void startListening(int backlog = 10);

    TcpStream acceptConnection();

    TcpStream acceptConnection(const SocketAddr& addr); // 可以获取客户端的ip端口

private:
    TcpStream acceptImpl(sockaddr* addr, socklen_t* len);
};

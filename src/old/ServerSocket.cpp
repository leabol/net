#include "../include/ServerSocket.hpp"
#include "../include/address.hpp"
#include "../include/TcpStream.hpp"

#include <sys/socket.h>
#include <netinet/in.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

void ServerSocket::bindTo(const SocketAddr& addr) {
    auto [sock_addr, len] = addr.endpoint();
    if (bind(socket_, sock_addr, len) == -1) {
        throw std::runtime_error("bind failed: " + std::string(strerror(errno)));
    }
}

void ServerSocket::startListening(int backlog) {
    if (listen(socket_, backlog) == -1) {
        throw std::runtime_error("listen failed: " + std::string(strerror(errno)));
    }
}

TcpStream ServerSocket::acceptConnection() {
    return acceptImpl(nullptr, nullptr);
}

TcpStream ServerSocket::acceptConnection(const SocketAddr& addr) {
    auto [sock_addr, len] = addr.endpoint();
    return acceptImpl(sock_addr, &len);
}

TcpStream ServerSocket::acceptImpl(sockaddr* addr, socklen_t* len) {
    sockaddr_in client{};
    socklen_t client_len = sizeof(client);
    
    int client_fd = accept4(socket_, addr ? addr : reinterpret_cast<sockaddr*>(&client), 
                            len ? len : &client_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            throw std::runtime_error("no pending connection");
        }
        throw std::runtime_error("accept failed: " + std::string(strerror(errno)));
    }
    return TcpStream(client_fd);
}

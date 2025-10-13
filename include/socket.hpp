#pragma once

#include <stdexcept>    
#include <string>       
#include <cstring>      
#include <cerrno>       
#include <netinet/in.h> 
#include <unistd.h>
#include <sys/socket.h>

#include "address.hpp"

class tcpConnection;

class Socket{
public:
    Socket() {
        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ == -1) {
            throw std::runtime_error("socket creation failed: "  + std::string(strerror(errno)));
        }
        int yes = 1;
        setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    }

    ~Socket() {
        if (socket_ != -1)
            close(socket_);
    }

    int get_fd() const {
        return socket_;
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : socket_(other.socket_) {
        other.socket_ = -1;
    };
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (socket_ != -1)
                close(socket_);
            socket_ = other.socket_;
            other.socket_ = -1;
        }
        return *this;
    }
protected:
    explicit Socket(int fd) : socket_(fd) {}
    int socket_{-1};
};

class tcpSocket:public Socket{
public:
    void Bind(const socketAddr& addr) {
        auto [sock_addr, len] = addr.get_addr();
        if (bind(socket_, sock_addr, len) == -1) {
            throw std::runtime_error("bind failed"+ std::string(strerror(errno)));
        }
    }

    void  Listen(int n = 10) {
        if (listen(socket_, n) == -1) {
            throw std::runtime_error("listen failed"+ std::string(strerror(errno)));
        }
    }

    void Connect(const socketAddr& addr) {
        auto [sock_addr, len] = addr.get_addr();
        if (connect(socket_, sock_addr, len) == -1) {
            throw std::runtime_error("connect is failed"+ std::string(strerror(errno)));
        }
    }

    tcpConnection Accept();

    tcpConnection Accept(const socketAddr& addr);

private:
    tcpConnection AcceptImpl(sockaddr* addr, socklen_t* len);
};

class tcpConnection:public Socket{
public:
    explicit tcpConnection(int fd) : Socket(fd) {}
    
    bool Send(const std::string& buff) {
        size_t total_send = 0;
        const char *data = buff.data();
        size_t buff_size = buff.size();
        while (total_send < buff_size) {
            ssize_t send_byte = send(socket_, data + total_send, buff_size - total_send,0);
            if (send_byte == -1) {
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                return false;
            }
            total_send += send_byte;
        }
        return true;
    }

    ssize_t Recv(char *buff, size_t len) {
        ssize_t recv_byte = recv(socket_, buff, len, 0);
        if (recv_byte == -1) {
            if (errno == EINTR) {
                return Recv(buff, len);
            }
            return -1;
        }
        return recv_byte;
    }
};

inline tcpConnection tcpSocket::Accept() {
    return AcceptImpl(nullptr, nullptr);
}

inline tcpConnection tcpSocket::Accept(const socketAddr& addr) {
    auto [sock_addr, len] = addr.get_addr();
    return AcceptImpl(sock_addr, &len);
}

inline tcpConnection tcpSocket::AcceptImpl(sockaddr* addr, socklen_t* len) {
    int client = accept(socket_, addr, len);
    if (client == -1) {
        throw std::runtime_error("accept failed" + std::string(strerror(errno)));
    }
    return tcpConnection(client);
}
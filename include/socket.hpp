#pragma once

#include <stdexcept>
#include <string>
#include <cstring>
#include <cerrno>
#include <optional>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>

#include "address.hpp"

class TcpConnection;

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

    int getFd() const {
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

class TcpSocket:public Socket{
public:
    void bindTo(const SocketAddr& addr);

    void startListening(int backlog = 10);

    TcpConnection connectTo(const SocketAddr& addr);

    TcpConnection acceptConnection();

    TcpConnection acceptConnection(const SocketAddr& addr);

private:
    TcpConnection acceptImpl(sockaddr* addr, socklen_t* len);

    int releaseFd() {
        int fd = socket_;
        socket_ = -1;
        return fd;
    }
};

class TcpConnection:public Socket{
public:
    explicit TcpConnection(int fd) : Socket(fd) {}
    
    bool sendAll(const std::string& buff) {
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

    std::optional<std::string> recvString(size_t max_len = 4096) {
        std::string data(max_len, '\0');
        ssize_t n = recvRaw(data.data(), max_len);
        if (n == 0) {
            return std::nullopt;
        }
        if (n < 0) {
            throw std::runtime_error("recv failed" + std::string(strerror(errno)));
        }
        data.resize(static_cast<size_t>(n));
        return data;
    }

    ssize_t recvRaw(char *buff, size_t len) {
        ssize_t recv_byte = recv(socket_, buff, len, 0);
        if (recv_byte == -1) {
            if (errno == EINTR) {
                return recvRaw(buff, len);
            }
            return -1;
        }
        return recv_byte;
    }
};

inline TcpConnection TcpSocket::acceptConnection() {
    return acceptImpl(nullptr, nullptr);
}

inline TcpConnection TcpSocket::acceptConnection(const SocketAddr& addr) {
    auto [sock_addr, len] = addr.endpoint();
    return acceptImpl(sock_addr, &len);
}

inline TcpConnection TcpSocket::acceptImpl(sockaddr* addr, socklen_t* len) {
    int client = accept(socket_, addr, len);
    if (client == -1) {
        throw std::runtime_error("accept failed" + std::string(strerror(errno)));
    }
    return TcpConnection(client);
}

inline void TcpSocket::bindTo(const SocketAddr& addr) {
    auto [sock_addr, len] = addr.endpoint();
    if (bind(socket_, sock_addr, len) == -1) {
        throw std::runtime_error("bind failed" + std::string(strerror(errno)));
    }
}

inline void TcpSocket::startListening(int backlog) {
    if (listen(socket_, backlog) == -1) {
        throw std::runtime_error("listen failed" + std::string(strerror(errno)));
    }
}

inline TcpConnection TcpSocket::connectTo(const SocketAddr& addr) {
    auto [sock_addr, len] = addr.endpoint();
    if (connect(socket_, sock_addr, len) == -1) {
        throw std::runtime_error("connect failed" + std::string(strerror(errno)));
    }
    return TcpConnection(releaseFd());
}
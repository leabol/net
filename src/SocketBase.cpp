#include "../include/SocketBase.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <iostream>

SocketBase::SocketBase(bool nonblock) {
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == -1) {
        throw std::runtime_error("socket creation failed: " + std::string(strerror(errno)));
    }
    int yes = 1;
    setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    if (nonblock) {
        setNonBlock();
    }
}

SocketBase::~SocketBase() {
    if (socket_ != -1)
        close(socket_);
}

void SocketBase::setNonBlock() {
    int flags = fcntl(socket_, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("failed to get file status flags: " + std::string(strerror(errno)));
    }
    if (fcntl(socket_, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("failed to set non-blocking mode: " + std::string(strerror(errno)));
    }
}

SocketBase& SocketBase::operator=(SocketBase&& other) noexcept {
    if (this != &other) {
        if (socket_ != -1)
            close(socket_);
        socket_ = other.socket_;
        other.socket_ = -1;
    }
    return *this;
}

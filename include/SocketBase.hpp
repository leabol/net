#pragma once

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

/**
 * @brief Base class for socket fd management
 * 
 * Handles fd lifecycle, destruction, and non-blocking mode setup.
 * Pure fd-level operations, no network operations.
 * Default: blocking mode. Use nonblock=true for non-blocking sockets.
 */
class SocketBase {
public:
    explicit SocketBase(bool nonblock = false); 
    ~SocketBase();

    int getFd() const { return socket_; }

    void setNonBlock();

    // Non-copyable
    SocketBase(const SocketBase&) = delete;
    SocketBase& operator=(const SocketBase&) = delete;

    // Movable
    SocketBase(SocketBase&& other) noexcept : socket_(other.socket_) {
        other.socket_ = -1;
    }
    SocketBase& operator=(SocketBase&& other) noexcept;

protected:
    explicit SocketBase(int fd) : socket_(fd) {}
    
    int releaseFd() {
        int fd = socket_;
        socket_ = -1;
        return fd;
    }

    int socket_{-1};
};

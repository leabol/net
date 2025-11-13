#pragma once

#include "SocketBase.hpp"

#include <cerrno>
#include <optional>
#include <string>

/**
 * @brief Established TCP data stream for bidirectional communication
 * 
 * Represents an active TCP connection (whether initiated by client or accepted by server).
 * Used to send and receive data.
 */
class TcpStream : public SocketBase {
public:
    explicit TcpStream(int fd) : SocketBase(fd) {}

    bool sendAll(const std::string& buff);

    std::optional<std::string> recvString(size_t max_len = 4096);

    ssize_t recvRaw(char* buff, size_t len);

    /**
     * @brief Release ownership of fd
     * 
     * Prevents destructor from closing the fd. Used when fd is closed externally.
     */
    void release() {
        socket_ = -1;
    }
};

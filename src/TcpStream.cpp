#include "../include/TcpStream.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <optional>
#include <string>

bool TcpStream::sendAll(const std::string& buff) {
    size_t total_send = 0;
    const char* data = buff.data();
    size_t buff_size = buff.size();
    while (total_send < buff_size) {
        ssize_t send_byte = send(socket_, data + total_send, buff_size - total_send, 0);
        if (send_byte == -1) {
            // EINTR: interrupted by signal, retry
            if (errno == EINTR) {
                continue;
            }
            // EAGAIN/EWOULDBLOCK: send buffer full (non-blocking mode)
            // For ET mode: return and wait for EPOLLOUT event
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return false;  // ← 改成 false，表示需要重试
            }
            // Real error
            return false;
        }
        if (send_byte == 0) {
            // Unexpected: send() returned 0
            return false;
        }
        total_send += send_byte;
    }
    return true;  // 全部发送成功
}

std::optional<std::string> TcpStream::recvString(size_t max_len) {
    std::string data(max_len, '\0');
    ssize_t n = recvRaw(data.data(), max_len);
    if (n == 0) {
        return std::nullopt;
    }
    if (n < 0) {
        throw std::runtime_error("recv failed: " + std::string(strerror(errno)));
    }
    data.resize(static_cast<size_t>(n));
    return data;
}

ssize_t TcpStream::recvRaw(char* buff, size_t len) {
    while (true) {
        ssize_t recv_byte = recv(socket_, buff, len, 0);
        if (recv_byte == -1) {
            if (errno == EINTR) {
                continue;  // Signal interruption, retry
            }
            // Non-blocking socket: EAGAIN/EWOULDBLOCK means no data available right now
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return -1;  // Let caller inspect errno and break the read loop
            }
            return -1;  // Propagate real error
        }
        return recv_byte;
    }
}

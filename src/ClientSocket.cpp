#include "../include/ClientSocket.hpp"
#include "../include/address.hpp"
#include "../include/TcpStream.hpp"

#include <sys/socket.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

TcpStream ClientSocket::connectTo(const SocketAddr& addr) {
    auto [sock_addr, len] = addr.endpoint();
    if (connect(socket_, sock_addr, len) == -1) {
        if (errno == EINPROGRESS) {
            // Non-blocking connect is in progress; caller must poll for writability and
            // verify the outcome with getsockopt(..., SO_ERROR, ...).
            return TcpStream(releaseFd());
        }
        throw std::runtime_error("connect failed: " + std::string(strerror(errno)));
    }
    return TcpStream(releaseFd());
}

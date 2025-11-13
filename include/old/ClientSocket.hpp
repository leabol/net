#pragma once

#include "SocketBase.hpp"

class SocketAddr;
class TcpStream;

/**
 * @brief Client-side socket for establishing TCP connections
 * 
 * Used to connect to a remote server.
 * Default: blocking mode for synchronous recv.
 */
class ClientSocket : public SocketBase {
public:
    ClientSocket() : SocketBase(false) {}  // ← 客户端用阻塞（默认）

    TcpStream connectTo(const SocketAddr& addr);
};

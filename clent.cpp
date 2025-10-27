#include "include/ClientSocket.hpp"
#include "include/TcpStream.hpp"
#include "include/address.hpp"

#include <iostream>
#include <string>

int main () 
{
    SocketAddr addr("localhost", "8090");

    ClientSocket sockfd;
    TcpStream connet_fd = sockfd.connectTo(addr);
    // ✓ 已是阻塞模式，无需手动设置

    while (true) {
        std::string msg;
        std::cin >> msg;

        if (!connet_fd.sendAll(msg)) {
            std::cout << "send failed\n";
        }

        auto res = connet_fd.recvString();
        if (!res.has_value()) {
            std::cout << "peer closed connection\n";
        } else {
            std::cout << *res << std::endl;
        }
    }
}
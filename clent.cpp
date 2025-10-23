#include "include/socket.hpp"
#include "include/address.hpp"

#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>

int main () 
{
    SocketAddr addr("localhost", "8090");

    TcpSocket sockfd;
    TcpConnection connet_fd = sockfd.connectTo(addr);

    // auto res = connet_fd.recvString();
    // if (!res.has_value()) {
    //     std::cout << "peer closed connection\n";
    // } else {
    //     std::cout << *res << std::endl;
    // }

    while (true) {
        std::string msg;
        std::cin >> msg;

        if (!connet_fd.sendAll(msg)) {
            std::cout << "connet faile\n";
        }

        auto res = connet_fd.recvString();
        if (!res.has_value()) {
            std::cout << "peer closed connection\n";
        } else {
            std::cout << *res << std::endl;
        }
    }
}
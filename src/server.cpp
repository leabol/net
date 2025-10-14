#include "../include/socket.hpp"
#include "../include/address.hpp"

#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include <cstring>


int main() {
    SocketAddr addr = SocketAddr::for_server("8090");

    TcpSocket sockfd;
    sockfd.bindTo(addr);

    sockfd.startListening();

    std::cout << "Server listening on port 8090..." << std::endl;

    while (true) {
        TcpConnection clientfd = sockfd.acceptConnection();

        std::string msg = "this is the world";
        clientfd.sendAll(msg);
        auto res = clientfd.recvString();
        if (!res.has_value()) {
            std::cout << "peer closed connection\n";
        }else {
            std::cout << "Received: " << *res; 
            clientfd.sendAll("bye!!!");;
        }
    }

    return 0;
}
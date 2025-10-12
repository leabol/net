#include "../include/socket.hpp"
#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include <cstring>

int main() {
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // 监听所有地址

    int ret = getaddrinfo(nullptr, "8090", &hints, &res);
    if (ret != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(ret) << std::endl;
        return 1;
    }

    tcpSocket sockfd;

    sockfd.Bind(res->ai_addr, res->ai_addrlen);

    freeaddrinfo(res);

    sockfd.Listen();

    std::cout << "Server listening on port 8090..." << std::endl;

    while (true) {
        tcpConnection clientfd = sockfd.Accept();
  
        char buff[100] = {0};
        ssize_t n = read(clientfd.get_fd(), buff, sizeof(buff)-1);
        if (n > 0) {
            buff[n] = '\0';
            std::cout << "Received: " << buff << std::endl;
            const char *reply = "world";
            write(clientfd.get_fd(), reply, strlen(reply));
        }
    }

    return 0;
}
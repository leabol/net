#include "include/socket.hpp"
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

int main () 
{
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo("localhost", "8090", &hints, &res);

    if (err != 0) {
        std::cerr << "getaddrinfo: " << gai_strerror(err) << std::endl; 
        return 1;
    }
    tcpSocket sockfd;
    sockfd.Connect(res->ai_addr, res->ai_addrlen);

    // 发送数据
    const char *msg = "hello";
    write(sockfd.get_fd(), msg, strlen(msg));

    // 接收服务端回复
    char buff[100] = {0};
    ssize_t n = read(sockfd.get_fd(), buff, sizeof(buff)-1);
    if (n > 0) {
        buff[n] = '\0';
        std::cout << buff << std::endl;
    } else {
        std::cerr << "read failed" << std::endl;
    }

    freeaddrinfo(res);

}
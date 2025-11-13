#include "../include/Socket.hpp"
#include "../include/InetAddress.hpp"
#include "../include/Log.hpp"

#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace Server;

int main(int argc, char** argv) {
    Server::initLogger();

    std::string host = (argc >= 2) ? argv[1] : std::string{"127.0.0.1"};
    std::string port = (argc >= 3) ? argv[2] : std::string{"8091"};

    try {
        Socket cli;
        InetAddress dst(host, port);
        // cli.connect(dst);
        cli.connect(host, port);
        LOG_INFO("connected to {}:{}", host, port);

        std::string line;
        std::cout << "type and press enter (Ctrl-D to quit)" << std::endl;
        while (std::getline(std::cin, line)) {
            line.push_back('\n');
            ssize_t n = ::write(cli.fd(), line.data(), line.size());
            if (n < 0) {
                LOG_ERROR("write failed: {}", std::strerror(errno));
                break;
            }

            char buf[1024];
            ssize_t m = ::read(cli.fd(), buf, sizeof(buf));
            if (m > 0) {
                std::cout.write(buf, m);
                std::cout.flush();
            } else if (m == 0) {
                LOG_WARN("server closed");
                break;
            } else {
                if (errno == EINTR) continue;
                LOG_ERROR("read failed: {}", std::strerror(errno));
                break;
            }
        }
    } catch (const std::exception& ex) {
        LOG_CRITICAL("client fatal: {}", ex.what());
        return 1;
    }
    return 0;
}

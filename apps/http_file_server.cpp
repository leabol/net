#include <filesystem>
#include <string>

#include "EventLoop.hpp"
#include "InetAddress.hpp"
#include "Log.hpp"
#include "http/HttpServer.hpp"

int main(int argc, char** argv) {
    Server::initLogger();

    const std::string           port = (argc > 1) ? argv[1] : std::string{"9200"};
    const std::filesystem::path storageDir =
        (argc > 2) ? std::filesystem::path{argv[2]} : std::filesystem::path{"storage"};
    const std::filesystem::path staticDir =
        (argc > 3) ? std::filesystem::path{argv[3]} : std::filesystem::path{"www"};

    Server::EventLoop   loop;
    Server::InetAddress listenAddr(port);
    Http::HttpServer    httpServer(&loop, listenAddr, storageDir, staticDir);

    httpServer.start();
    LOG_INFO("http file server listening on port {} (storage={}, static={})",
             port,
             storageDir.string(),
             staticDir.string());

    loop.loop(1000);
    return 0;
}

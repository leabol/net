#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "EventLoop.hpp"
#include "InetAddress.hpp"
#include "TcpServer.hpp"
#include "http/HttpRequest.hpp"

namespace Http {

class HttpServer {
  public:
    HttpServer(Server::EventLoop*         loop,
               const Server::InetAddress& listenAddr,
               std::filesystem::path      storageDir,
               std::filesystem::path      staticDir);

    void start();

  private:
    void onConnection(const Server::TcpServer::TcpConnectionPtr& conn);
    void onMessage(const Server::TcpServer::TcpConnectionPtr& conn, std::string& data);

    void handleRequest(const Server::TcpServer::TcpConnectionPtr& conn, const HttpRequest& req);

    void handleGet(const Server::TcpServer::TcpConnectionPtr& conn, const HttpRequest& req);
    void handlePost(const Server::TcpServer::TcpConnectionPtr& conn, const HttpRequest& req);
    void handleDelete(const Server::TcpServer::TcpConnectionPtr& conn, const HttpRequest& req);

    void replyStaticFile(const Server::TcpServer::TcpConnectionPtr& conn,
                         const std::filesystem::path&               relativePath);
    void replyFileList(const Server::TcpServer::TcpConnectionPtr& conn);
    void replyDownload(const Server::TcpServer::TcpConnectionPtr& conn, std::string_view fileName);
    void handleUpload(const Server::TcpServer::TcpConnectionPtr& conn, const HttpRequest& req);
    void handleRemove(const Server::TcpServer::TcpConnectionPtr& conn, std::string_view fileName);

    Server::TcpServer                          server_;
    std::unordered_map<int, ConnectionContext> contexts_;
    std::filesystem::path                      storageDir_;
    std::filesystem::path                      staticDir_;
};

}  // namespace Http

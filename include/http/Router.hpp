#pragma once
#include <functional>
#include <map>
#include <string>

#include "TcpServer.hpp"
#include "http/HttpRequest.hpp"
namespace HTTP {

class Router {
  public:
    using route_key = std::pair<std::string, std::string>;
    using handler   = std::function<void(const Server::TcpServer::TcpConnectionPtr& conn,
                                       const Http::HttpRequest&                   req)>;

    Router(const Router&)            = delete;
    Router& operator=(const Router&) = delete;
    Router(Router&&)                 = delete;
    Router& operator=(Router&&)      = delete;

    void Get(const std::string path, const handler);
    void Post(const std::string path, const handler);
    void Delete(const std::string path, const handler);

  private:
    std::map<route_key, handler> route_map;
};

}  // namespace HTTP

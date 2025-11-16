#pragma once

#include <string>
#include <unordered_map>

namespace Http {

struct HttpRequest {
    std::string                                  method;
    std::string                                  path;
    std::string                                  version;
    std::unordered_map<std::string, std::string> headers;
    std::string                                  body;
};

struct ConnectionContext {
    std::string buffer;
};

bool parseHttpRequest(ConnectionContext& ctx, HttpRequest& req);

}  // namespace Http

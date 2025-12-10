#pragma once

#include <string>

#include "http/HttpRequest.hpp"

namespace Http {

struct ConnectionContext {
    std::string buffer;
};

bool parseHttpRequest(ConnectionContext& ctx, HttpRequest& req);

}  // namespace Http

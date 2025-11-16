#include "http/HttpResponse.hpp"

#include <sstream>

namespace Http {

void HttpResponse::setStatus(int code, std::string reason) {
    statusCode_   = code;
    reasonPhrase_ = std::move(reason);
}

void HttpResponse::setHeader(std::string key, std::string value) {
    headers_[std::move(key)] = std::move(value);
}

void HttpResponse::setBody(std::string body) {
    body_ = std::move(body);
}

void HttpResponse::setContentType(std::string mime) {
    headers_["Content-Type"] = std::move(mime);
}

std::string HttpResponse::serialize(bool keepAlive) const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << statusCode_ << ' ' << reasonPhrase_ << "\r\n";

    if (headers_.find("Content-Length") == headers_.end()) {
        oss << "Content-Length: " << body_.size() << "\r\n";
    }

    if (headers_.find("Connection") == headers_.end()) {
        oss << "Connection: " << (keepAlive ? "keep-alive" : "close") << "\r\n";
    }

    for (const auto& [key, value] : headers_) {
        oss << key << ": " << value << "\r\n";
    }
    oss << "\r\n";
    oss << body_;
    return oss.str();
}

}  // namespace Http

#pragma once

#include <string>

#include "HttpRequest.hpp"
namespace Http {

class HttpParser {
  public:
    enum class FeedState {
        NEED_MORE,
        COMPLETE,
        ERROR,
    };
    FeedState          feed(std::string_view data);
    const HttpRequest& getRequest() const;
    void               resetParser(bool keepBuffer = true);
    bool               isKeepAlive() const;

  private:
    enum class HttpParseState {
        REQUEST_LINE_PENDING,  // 等待解析请求行
        HEADER_PENDING,        // 等待解析请求头
        HEADER_COMPLETE,       // 请求头解析结束
        BODY_CONTENT_LENGTH,   // 解析body
        REQUEST_COMPLETE,      // 解析结束
        PARSE_ERROR
    };
    HttpParseState state_{HttpParseState::REQUEST_LINE_PENDING};
    size_t         content_length{0};
    size_t         body_received{0};
    bool           keep_alive{false};
    std::string    recv_buff_;
    HttpRequest    request_;
};

struct ConnectionContext {
    HttpParser parser;
};

}  // namespace Http

#include "../../include/http/HttpParser.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace Http {

HttpParser::FeedState HttpParser::feed(std::string_view data) {
    recv_buff_.append(data.data(), data.size());
    if (state_ == HttpParseState::REQUEST_LINE_PENDING) {
        auto pos = recv_buff_.find("\r\n");
        if (pos == std::string::npos) {
            return FeedState::NEED_MORE;
        }

        std::string_view request_line{recv_buff_.data(), pos};  // 使用视图来减少copy

        auto split1 = request_line.find(' ');
        auto split2 =
            (split1 == std::string::npos) ? std::string::npos : request_line.find(' ', split1 + 1);
        if (split1 == std::string::npos || split2 == std::string::npos) {
            return FeedState::ERROR;
        }
        request_.method  = std::string{request_line.substr(0, split1)};
        request_.path    = std::string{request_line.substr(split1 + 1, split2 - split1 - 1)};
        request_.version = std::string{request_line.substr(split2 + 1)};

        recv_buff_.erase(0, pos + 2);
        state_ = HttpParseState::HEADER_PENDING;
    }

    if (state_ == HttpParseState::HEADER_PENDING) {
        while (true) {
            auto pos = recv_buff_.find("\r\n");
            if (pos == std::string::npos) {
                return FeedState::NEED_MORE;
            }
            if (pos == 0) {
                recv_buff_.erase(0, 2);
                state_ = HttpParseState::HEADER_COMPLETE;
                break;
            }

            std::string_view header_line{recv_buff_.data(), pos};
            auto             split = header_line.find(':');
            if (split == std::string_view::npos) {
                return FeedState::ERROR;
            }
            std::string name{header_line.substr(0, split)};
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);

            auto val_start = split + 1;
            //  跳过:后的空格
            while (val_start < header_line.size() &&
                   (header_line[val_start] == ' ' || header_line[val_start] == '\t')) {
                ++val_start;
            }
            std::string value{header_line.substr(val_start)};

            request_.headers[name] = std::move(value);
            recv_buff_.erase(0, pos + 2);
        }
    }

    if (state_ == HttpParseState::HEADER_COMPLETE) {
        // 判断keep-alive
        auto        connection_it = request_.headers.find("connection");
        std::string conn_value =
            (connection_it != request_.headers.end()) ? connection_it->second : "";

        std::transform(conn_value.begin(), conn_value.end(), conn_value.begin(), ::tolower);
        if (request_.version == "HTTP/1.1") {
            // HTTP/1.1 默认 keep-alive，除非显式关闭
            keep_alive = (conn_value != "close");
        } else {
            // HTTP/1.0 默认关闭，除非显式 keep-alive
            keep_alive = (conn_value == "keep-alive");
        }

        if (request_.headers.find("transfer-encoding") != request_.headers.end()) {
            return FeedState::ERROR;  // 不支持 chunked
        }
        //  content-length
        auto content_length_it = request_.headers.find("content-length");
        if (content_length_it != request_.headers.end()) {
            try {
                content_length = static_cast<size_t>(std::stoul(content_length_it->second));
            } catch (...) {
                return FeedState::ERROR;
            }
        } else {
            //  无body
            state_ = HttpParseState::REQUEST_COMPLETE;
            return FeedState::COMPLETE;
        }

        state_ = HttpParseState::BODY_CONTENT_LENGTH;
    }

    if (state_ == HttpParseState::BODY_CONTENT_LENGTH) {
        if (content_length > recv_buff_.size()) {
            return FeedState::NEED_MORE;
        }
        request_.body.assign(recv_buff_.data(), content_length);
        recv_buff_.erase(0, content_length);
        state_ = HttpParseState::REQUEST_COMPLETE;
        return FeedState::COMPLETE;
    }
    return FeedState::NEED_MORE;
}
const HttpRequest& HttpParser::getRequest() const {
    return request_;
}
void HttpParser::resetParser(bool keepBuffer) {
    state_         = HttpParseState::REQUEST_LINE_PENDING;
    content_length = 0;
    keep_alive     = false;
    request_       = HttpRequest{};
    if (!keepBuffer) {
        recv_buff_.clear();
    }
}
bool HttpParser::isKeepAlive() const {
    return keep_alive;
}
}  // namespace Http

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>

#include "http/HttpRequest.hpp"

namespace {

std::string trim(std::string_view input) {
    size_t begin = 0;
    size_t end   = input.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(input[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
        --end;
    }
    return std::string(input.substr(begin, end - begin));
}

std::string toLower(std::string_view text) {
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return lowered;
}

}  // namespace

namespace Http {

bool parseHttpRequest(ConnectionContext& ctx, HttpRequest& req) {
    auto& buffer   = ctx.buffer;
    auto  boundary = buffer.find("\r\n\r\n");
    if (boundary == std::string::npos) {
        return false;
    }

    const size_t      headerEnd = boundary + 4;
    const std::string headerRaw = buffer.substr(0, boundary);

    size_t      lineEnd = headerRaw.find("\r\n");
    std::string requestLine =
        (lineEnd == std::string::npos) ? headerRaw : headerRaw.substr(0, lineEnd);

    std::istringstream lineStream(requestLine);
    if (!(lineStream >> req.method >> req.path >> req.version)) {
        buffer.erase(0, headerEnd);
        return false;
    }

    std::transform(req.method.begin(), req.method.end(), req.method.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    size_t searchPos = (lineEnd == std::string::npos) ? headerRaw.size() : lineEnd + 2;
    while (searchPos < headerRaw.size()) {
        size_t nextEnd = headerRaw.find("\r\n", searchPos);
        if (nextEnd == std::string::npos) {
            nextEnd = headerRaw.size();
        }
        std::string line     = headerRaw.substr(searchPos, nextEnd - searchPos);
        auto        colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key   = toLower(trim(std::string_view(line.data(), colonPos)));
            std::string value = trim(std::string_view(line).substr(colonPos + 1));
            req.headers[key]  = value;
        }
        searchPos = nextEnd + 2;
    }

    size_t contentLength = 0;
    if (auto it = req.headers.find("content-length"); it != req.headers.end()) {
        std::string lenStr = trim(it->second);
        try {
            contentLength = static_cast<size_t>(std::stoul(lenStr));
        } catch (...) {
            contentLength = 0;
        }
    }

    if (buffer.size() < headerEnd + contentLength) {
        return false;
    }

    req.body = buffer.substr(headerEnd, contentLength);
    buffer.erase(0, headerEnd + contentLength);
    return true;
}

}  // namespace Http

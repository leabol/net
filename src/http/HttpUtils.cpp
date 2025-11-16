#include "http/HttpUtils.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace {
constexpr int           kHexBase             = 16;
constexpr unsigned char kJsonPrintableFloor  = 0x20;
constexpr std::size_t   kUnicodeEscapeBufLen = 7;
}  // namespace

namespace Http {

std::string stripQuery(const std::string& path) {
    auto pos = path.find('?');
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(0, pos);
}

std::string urlDecode(std::string_view input) {
    std::string result;
    result.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            const char hex[3] = {input[i + 1], input[i + 2], 0};
            char*      endPtr = nullptr;
            long       value  = std::strtol(hex, &endPtr, kHexBase);
            if (endPtr != nullptr && *endPtr == '\0') {
                result.push_back(static_cast<char>(value));
                i += 2;
                continue;
            }
        }
        if (input[i] == '+') {
            result.push_back(' ');
        } else {
            result.push_back(static_cast<char>(input[i]));
        }
    }
    return result;
}

std::string escapeJson(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < kJsonPrintableFloor) {
                    char buf[kUnicodeEscapeBufLen];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    return out;
}

std::string sanitizeFilename(std::string_view name) {
    std::filesystem::path p(name);
    auto                  clean = p.filename().string();
    clean.erase(
        std::remove_if(
            clean.begin(), clean.end(), [](unsigned char c) { return c == '/' || c == '\\'; }),
        clean.end());
    return clean;
}

}  // namespace Http

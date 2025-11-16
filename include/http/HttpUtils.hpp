#pragma once

#include <string>
#include <string_view>

namespace Http {

std::string stripQuery(const std::string& path);
std::string urlDecode(std::string_view input);
std::string escapeJson(std::string_view input);
std::string sanitizeFilename(std::string_view name);

}  // namespace Http

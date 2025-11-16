#pragma once

#include <string>
#include <unordered_map>

namespace Http {

class HttpResponse {
  public:
    static constexpr int kDefaultStatus = 200;

    HttpResponse() = default;

    void                      setStatus(int code, std::string reason);
    void                      setHeader(std::string key, std::string value);
    void                      setBody(std::string body);
    void                      setContentType(std::string mime);
    [[nodiscard]] std::string serialize(bool keepAlive) const;

  private:
    int                                          statusCode_{kDefaultStatus};
    std::string                                  reasonPhrase_{"OK"};
    std::unordered_map<std::string, std::string> headers_;
    std::string                                  body_;
};

}  // namespace Http

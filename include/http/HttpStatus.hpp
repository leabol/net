#pragma once

namespace Http {
namespace StatusCode {
inline constexpr int kOk                  = 200;
inline constexpr int kCreated             = 201;
inline constexpr int kBadRequest          = 400;
inline constexpr int kNotFound            = 404;
inline constexpr int kMethodNotAllowed    = 405;
inline constexpr int kInternalServerError = 500;
}  // namespace StatusCode
}  // namespace Http

#pragma once

#include <fmt/format.h>

#include <cstdint>
#include <expected>
#include <string>
#include <utility>

namespace schools {

enum class Errc : std::uint8_t {
  kInvalidArgument,  // HTTP 400
  kNotFound,         // HTTP 404
  kUnavailable,      // HTTP 503
  kDatabase,
  kNetwork,
  kParse,
  kInternal,
};

struct Error {
  Errc code{Errc::kInternal};
  std::string message;
};

// All fallible functions return Result<T> instead of throwing. Exceptions from
// external libraries (SOCI, libcurl) are converted to Error at the boundary.
template <typename T>
using Result = std::expected<T, Error>;
using Status = Result<void>;

template <typename... Args>
std::unexpected<Error> MakeError(Errc code, fmt::format_string<Args...> format,
                                 Args&&... args) {
  return std::unexpected<Error>(
      Error{code, fmt::format(format, std::forward<Args>(args)...)});
}

}  // namespace schools

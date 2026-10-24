#pragma once

#include <fmt/format.h>

#include <boost/system/error_code.hpp>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace schools {

enum class Errc : uint8_t {
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

// All fallible functions return Result<T> instead of throwing. Errors coming
// from Boost (Asio, Beast, MySQL, JSON) are wrapped into Error at the boundary
// with FromBoost(); exceptions are not used for control flow anywhere.
template <typename T>
using Result = std::expected<T, Error>;
using Status = Result<void>;

template <typename... Args>
std::unexpected<Error> MakeError(Errc code, fmt::format_string<Args...> format,
                                 Args&&... args) {
  return std::unexpected<Error>(
      Error{code, fmt::format(format, std::forward<Args>(args)...)});
}

inline Error FromBoost(Errc code, std::string_view context,
                       const boost::system::error_code& ec) {
  return Error{code, fmt::format("{}: {}", context, ec.message())};
}

}  // namespace schools

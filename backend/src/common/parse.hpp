#pragma once

#include <charconv>
#include <string_view>
#include <system_error>

#include "common/error.hpp"

namespace schools {

// Strict number parsing without exceptions (the whole string must be a number).
template <typename T>
Result<T> ParseNumber(std::string_view text, std::string_view name) {
  T value{};
  const auto [ptr, ec] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || ptr != text.data() + text.size()) {
    return MakeError(Errc::kInvalidArgument, "Invalid value for '{}': '{}'",
                     name, text);
  }
  return value;
}

}  // namespace schools

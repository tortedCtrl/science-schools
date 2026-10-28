#pragma once

#include <charconv>
#include <string_view>
#include <system_error>

#include "common/error.hpp"

namespace schools {

template <typename T>
Result<T> ParseNumber(std::string_view text, std::string_view name) {
  T value{};
  const auto [ptr, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || ptr != text.data() + text.size()) {
    return MakeError(Errc::kInvalidArgument, "invalid value for {}: {}", name,
                     text);
  }
  return value;
}

}  // namespace schools

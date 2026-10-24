#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace schools {

std::string UrlEncode(std::string_view input);
std::string UrlDecode(std::string_view input);

// Parses "a=1&b=2" (without the leading '?') into a map, percent-decoding
// keys and values.
std::unordered_map<std::string, std::string> ParseQueryString(
    std::string_view query);

}  // namespace schools

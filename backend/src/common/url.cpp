#include "common/url.hpp"

#include <cctype>
#include <charconv>

namespace schools {
namespace {

bool IsUnreserved(unsigned char c) {
  return std::isalnum(c) != 0 || c == '-' || c == '.' || c == '_' || c == '~';
}

}  // namespace

std::string UrlEncode(std::string_view input) {
  static constexpr std::string_view kHex{"0123456789ABCDEF"};
  std::string out;
  out.reserve(input.size());
  for (const char ch : input) {
    const auto c{static_cast<unsigned char>(ch)};
    if (IsUnreserved(c)) {
      out.push_back(ch);
    } else {
      out.push_back('%');
      out.push_back(kHex[c >> 4U]);
      out.push_back(kHex[c & 0xFU]);
    }
  }
  return out;
}

std::string UrlDecode(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '+') {
      out.push_back(' ');
    } else if (input[i] == '%' && i + 2 < input.size()) {
      unsigned value{0};
      const auto [ptr, ec] = std::from_chars(input.data() + i + 1,
                                             input.data() + i + 3, value, 16);
      if (ec == std::errc{} && ptr == input.data() + i + 3) {
        out.push_back(static_cast<char>(value));
        i += 2;
      } else {
        out.push_back(input[i]);
      }
    } else {
      out.push_back(input[i]);
    }
  }
  return out;
}

std::unordered_map<std::string, std::string> ParseQueryString(
    std::string_view query) {
  std::unordered_map<std::string, std::string> result;
  while (!query.empty()) {
    const auto amp{query.find('&')};
    const std::string_view pair{query.substr(0, amp)};
    query = amp == std::string_view::npos ? std::string_view{}
                                          : query.substr(amp + 1);
    if (pair.empty()) {
      continue;
    }
    const auto eq{pair.find('=')};
    const std::string key{UrlDecode(pair.substr(0, eq))};
    const std::string value{eq == std::string_view::npos
                                ? std::string{}
                                : UrlDecode(pair.substr(eq + 1))};
    result[key] = value;
  }
  return result;
}

}  // namespace schools

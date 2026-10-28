#include "common/url.hpp"

#include <cctype>

namespace schools {
namespace {

bool IsUnreserved(unsigned char character) {
  return std::isalnum(character) != 0 || character == '-' || character == '.' ||
         character == '_' || character == '~';
}

}  // namespace

std::string UrlEncode(std::string_view input) {
  static constexpr std::string_view kHex = "0123456789ABCDEF";

  std::string encoded;
  encoded.reserve(input.size());
  for (const char character : input) {
    const auto value = static_cast<unsigned char>(character);
    if (IsUnreserved(value)) {
      encoded.push_back(character);
    } else {
      encoded.push_back('%');
      encoded.push_back(kHex[value >> 4U]);
      encoded.push_back(kHex[value & 0x0FU]);
    }
  }
  return encoded;
}

}  // namespace schools

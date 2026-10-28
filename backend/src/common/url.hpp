#pragma once

#include <string>
#include <string_view>

namespace schools {

// Percent-encodes a query parameter value. OpenAlex filters use '|' and ':'
// heavily, so the value cannot be inserted into the URL verbatim.
std::string UrlEncode(std::string_view input);

}  // namespace schools

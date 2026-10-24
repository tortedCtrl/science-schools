#pragma once

#include <string_view>

#include "common/error.hpp"
#include "domain/models.hpp"

namespace schools::openalex {

// Converts an OpenAlex /works response body into a WorksPage.
// Only authorships with an institution in `country_code` are kept, and works
// without any such author are dropped. Malformed JSON yields Errc::kParse.
Result<domain::WorksPage> ParseWorksPage(std::string_view body,
                                         std::string_view country_code);

}  // namespace schools::openalex

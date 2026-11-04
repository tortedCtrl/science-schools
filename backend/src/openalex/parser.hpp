#pragma once

#include <string_view>
#include <unordered_set>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"

namespace schools::openalex {

// Converts one OpenAlex /authors response page into domain authors.
Result<std::vector<domain::Author>> ParseAuthors(std::string_view body);

// Converts one OpenAlex /works response page into domain works. Only the
// authors listed in `selected_authors` are kept in each work.
Result<std::vector<domain::Work>> ParseWorks(
    std::string_view body,
    const std::unordered_set<std::string>& selected_authors);

}  // namespace schools::openalex

#pragma once

#include <cstdint>
#include <string>

namespace schools::domain {

using AuthorId = std::int32_t;
using CommunityId = std::int32_t;
using RunId = std::int64_t;
using Weight = std::int64_t;
using ArticleId = std::string;

}  // namespace schools::domain
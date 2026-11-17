#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schools::domain {

struct AuthorSummary {
  std::int32_t id{0};
  std::string openalex_id;
  std::string name;
  std::optional<std::int32_t> community_id;
  std::optional<std::string> institution_name;
  std::int64_t weighted_degree{0};
};
}
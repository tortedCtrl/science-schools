#pragma once

#include <optional>
#include <string>
#include "domain/common/types.hpp"

namespace schools::domain {

struct AuthorSummary {
    AuthorId id{0};
    std::string openalex_id;
    std::string name;
    std::optional<CommunityId> community_id;
    std::optional<std::string> institution_name;
    Weight weighted_degree{0};
};

}  // namespace schools::domain
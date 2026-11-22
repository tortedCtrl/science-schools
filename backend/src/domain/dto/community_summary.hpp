#pragma once

#include "domain/common/types.hpp"

namespace schools::domain {

struct CommunitySummary {
    CommunityId community_id{0};
    std::int64_t member_count{0};
};

}  // namespace schools::domain

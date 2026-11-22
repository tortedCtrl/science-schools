#pragma once

#include "domain/common/types.hpp"

namespace schools::domain {

struct Membership {
    RunId run_id{0};
    AuthorId author_id{0};
    CommunityId community_id{0};
};

}  // namespace schools::domain
#pragma once

#include <string>
#include "domain/common/types.hpp"
#include "domain/common/enums.hpp"

namespace schools::domain {

struct CommunityTopic {
    RunId run_id{0};
    CommunityAlgorithm algorithm{CommunityAlgorithm::kLeiden};
    CommunityId community_id{0};
    std::string topic_id;
    std::string topic_name;
    double weight{0.0};
    std::int32_t article_count{0};
};

}  // namespace schools::domain
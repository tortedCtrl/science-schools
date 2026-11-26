#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "domain/dto/topic_info.hpp"
#include "domain/dto/cluster_by_topic.hpp"
#include "domain/db/edge.hpp"

namespace schools::domain {

struct ClusterContent {
    std::int32_t community_id{0};
    std::int32_t member_count{0};
    std::vector<AuthorInCluster> authors;
    std::vector<TopicInfo> topics;
    std::vector<Edge> links;
};

}  // namespace schools::domain
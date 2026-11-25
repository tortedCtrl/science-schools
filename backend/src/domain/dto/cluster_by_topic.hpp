#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace schools::domain {

struct AuthorInCluster {
    std::int32_t author_id{0};
    std::string openalex_id;
    std::string name;
};

struct ClusterByTopic {
    std::int32_t community_id{0};
    std::int64_t member_count{0};
    std::vector<AuthorInCluster> authors;
};

}  // namespace schools::domain
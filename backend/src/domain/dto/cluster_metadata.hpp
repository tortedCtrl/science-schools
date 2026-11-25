#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace schools::domain {

struct ClusterMetadata {
    std::int64_t run_id{0};
    std::string algorithm;
    std::int32_t community_id{0};
    std::int64_t member_count{0};
};

}  // namespace schools::domain
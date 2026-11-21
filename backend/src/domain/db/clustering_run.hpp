#pragma once

#include <string>
#include <chrono>
#include "domain/common/types.hpp"
#include "domain/common/enums.hpp"

namespace schools::domain {

struct ClusteringRun {
    RunId id{0};
    CommunityAlgorithm algorithm{CommunityAlgorithm::kLeiden};
    std::chrono::system_clock::time_point created_at;
};

}  // namespace schools::domain
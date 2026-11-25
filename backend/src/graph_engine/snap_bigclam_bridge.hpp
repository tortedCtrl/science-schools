#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace schools::graph::detail {

struct SnapEdge {
  std::int32_t source;
  std::int32_t target;
};

struct SnapMembership {
  std::int32_t author_id;
  std::int32_t community_id;
};

struct SnapRunResult {
  std::vector<SnapMembership> memberships;
  std::string error;
};

SnapRunResult RunSnapBigClam(const std::vector<SnapEdge>& edges,
                            int communities, int threads);

}  // namespace schools::graph::detail

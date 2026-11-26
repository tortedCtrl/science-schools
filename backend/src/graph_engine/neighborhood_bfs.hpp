#pragma once
#include <cstddef>
#include <cstdint>
#include "domain/models.hpp"

namespace schools::graph {
domain::GraphData NeighborhoodBfs(const domain::GraphData& full,
                                  std::int32_t author_id, int depth,
                                  std::size_t cap);
}  // namespace schools::graph

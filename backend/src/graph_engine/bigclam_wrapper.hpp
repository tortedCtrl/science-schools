#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"

namespace schools::graph {

// Runs the Stanford SNAP BigCLAM executable on an unweighted, undirected graph.
// The algorithm can assign one author to multiple communities.
class BigClamWrapper {
 public:
  static Result<std::vector<domain::Membership>> Run(
      std::span<const domain::Edge> edges,
      const std::filesystem::path& executable, int communities, int threads);
};

}  // namespace schools::graph

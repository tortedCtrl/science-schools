#pragma once

#include <span>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"

namespace schools::graph {

// Runs Stanford SNAP BigCLAM directly through its C++ library, without a
// subprocess or intermediate edge-list files. The graph is undirected and
// unweighted; one author can belong to multiple communities.
class BigClamWrapper {
 public:
  static Result<std::vector<domain::Membership>> Run(
      std::span<const domain::Edge> edges, int communities, int threads);
};

}  // namespace schools::graph

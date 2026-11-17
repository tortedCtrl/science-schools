#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "domain/models/graph.hpp"
#include "domain/views/author_summary.hpp"

namespace schools::domain {

struct GraphData {
  std::vector<AuthorSummary> nodes;
  std::vector<Edge> links;
};
}
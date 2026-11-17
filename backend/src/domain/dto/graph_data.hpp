#pragma once

#include <vector>
#include "domain/db/edge.hpp"
#include "domain/dto/author_summary.hpp"

namespace schools::domain {

struct GraphData {
    std::vector<AuthorSummary> nodes;
    std::vector<Edge> links;
};

}  // namespace schools::domain
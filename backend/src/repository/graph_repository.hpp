#pragma once

#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

class GraphRepository {
 public:
  explicit GraphRepository(Database& database) : database_(database) {}

  Result<std::vector<domain::Edge>> ListEdges();
  Result<domain::GraphData> GetGraph();

 private:
  Database& database_;
};

}  // namespace schools::repository

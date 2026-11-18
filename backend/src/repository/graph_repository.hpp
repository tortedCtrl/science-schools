#pragma once

#include <string_view>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

class GraphRepository {
 public:
  explicit GraphRepository(Database& database) : database_(database) {}

  Result<std::vector<domain::Edge>> ListEdges();
  Result<domain::GraphData> GetGraph(std::string_view algorithm);
  Result<std::vector<domain::ArticleTopicInput>> GetArticleTopicInputs();

 private:
  Database& database_;
};

}  // namespace schools::repository

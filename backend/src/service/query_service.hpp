#pragma once

#include <cstdint>
#include <string>

#include "common/error.hpp"
#include "repository/author_repository.hpp"
#include "repository/graph_repository.hpp"

namespace schools::service {

// Read-only use cases used by the HTTP layer. Keeping JSON serialization here
// means the repositories never depend on the API representation.
class QueryService {
 public:
  QueryService(repository::AuthorRepository& authors,
               repository::GraphRepository& graph)
      : authors_{authors}, graph_{graph} {}

  Result<std::string> ListAuthorsJson();
  Result<std::string> GetAuthorCardJson(std::int32_t author_id);
  Result<std::string> ListEdgesJson();
  Result<std::string> GetGraphJson();

 private:
  repository::AuthorRepository& authors_;
  repository::GraphRepository& graph_;
};

}  // namespace schools::service

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "common/error.hpp"
#include "repository/author_repository.hpp"
#include "repository/community_repository.hpp"
#include "repository/graph_repository.hpp"

namespace schools::service {

// Read-only use cases used by the HTTP layer. Keeping JSON serialization here
// means the repositories never depend on the API representation.
class QueryService {
 public:
  QueryService(repository::AuthorRepository& authors,
              repository::GraphRepository& graph,
              repository::CommunityRepository& communities)
      : authors_{authors}, graph_{graph},  communities_{communities} {}

  Result<std::string> ListAuthorsJson();
  Result<std::string> GetAuthorCardJson(std::int32_t author_id);
  Result<std::string> ListEdgesJson();
  Result<std::string> GetGraphJson();
  Result<std::string> ListBigClamMembershipsJson();
  Result<std::string> ListCommunityTopicsJson(
      std::string_view algorithm = "leiden");

 private:
  repository::AuthorRepository& authors_;
  repository::GraphRepository& graph_;
  repository::CommunityRepository& communities_;
};

}  // namespace schools::service

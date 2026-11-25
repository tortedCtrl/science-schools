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
  Result<std::string> GetGraphJson(std::string_view algorithm = "leiden");
  Result<std::string> ListCommunitiesJson(std::string_view algorithm);
  Result<std::string> ListCommunityTopicsJson(std::string_view algorithm,
                                              std::int32_t community_id);
  Result<std::string> ListAllTopicsJson();
  Result<std::string> ListClusterMetadataJson(std::string_view algorithm);
  Result<std::string> ListClustersByTopicJson(std::string_view algorithm,
                                              std::string_view topic_id);
  Result<std::string> GetClusterContentJson(std::string_view algorithm,
                                            std::int32_t community_id);

 private:
  repository::AuthorRepository& authors_;
  repository::GraphRepository& graph_;
  repository::CommunityRepository& communities_;
};

}  // namespace schools::service


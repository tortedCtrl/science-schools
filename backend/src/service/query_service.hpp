#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <optional>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/author_repository.hpp"
#include "repository/community_repository.hpp"
#include "repository/graph_repository.hpp"

namespace schools::service {

// Read-side use cases for the frontend. Validates/clamps limits and reports
// Errc::kNotFound / Errc::kUnavailable, which the API layer maps to HTTP codes.
class QueryService {
 public:
  static constexpr int kMaxGraphNodes{2000};
  static constexpr int kMaxPageSize{500};
  static constexpr int kTopFields{5};

  QueryService(repository::AuthorRepository& authors,
               repository::CommunityRepository& communities,
               repository::GraphRepository& graph)
      : authors_{authors}, communities_{communities}, graph_{graph} {}

  boost::asio::awaitable<Result<domain::AuthorCommunity>> GetAuthorCommunity(
      std::int64_t author_id) const;

  boost::asio::awaitable<Result<domain::CommunityDetails>> GetCommunity(
      std::int32_t community_id, int limit, int offset) const;

  boost::asio::awaitable<Result<std::vector<domain::CommunitySummary>>>
  ListCommunities(int limit) const;

  // A bounded slice of the graph: the top `limit_nodes` authors by weighted
  // degree (optionally within one community) and the edges among them.
  boost::asio::awaitable<Result<domain::GraphSlice>> GetGraph(
      std::optional<std::int32_t> community_id, int limit_nodes,
      float min_weight) const;

 private:
  boost::asio::awaitable<Result<std::int64_t>> RequireActiveRun() const;

  repository::AuthorRepository& authors_;
  repository::CommunityRepository& communities_;
  repository::GraphRepository& graph_;
};

}  // namespace schools::service

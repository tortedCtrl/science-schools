#include "service/query_service.hpp"

#include <algorithm>
#include <utility>

namespace schools::service {
namespace asio = boost::asio;

asio::awaitable<Result<std::int64_t>> QueryService::RequireActiveRun() const {
  auto run_id = co_await communities_.ActiveRunId();
  if (!run_id) {
    co_return std::unexpected(run_id.error());
  }
  if (!*run_id) {
    co_return MakeError(Errc::kUnavailable,
                        "No clustering run is available yet");
  }
  co_return **run_id;
}

asio::awaitable<Result<domain::AuthorCommunity>>
QueryService::GetAuthorCommunity(std::int64_t author_id) const {
  const auto run_id = co_await RequireActiveRun();
  if (!run_id) {
    co_return std::unexpected(run_id.error());
  }

  auto author = co_await authors_.Find(author_id);
  if (!author) {
    co_return std::unexpected(author.error());
  }
  if (!*author) {
    co_return MakeError(Errc::kNotFound, "Author {} not found", author_id);
  }

  const auto membership =
      co_await communities_.FindMembership(*run_id, author_id);
  if (!membership) {
    co_return std::unexpected(membership.error());
  }
  if (!*membership) {
    co_return MakeError(Errc::kNotFound, "Author {} is not part of the graph",
                        author_id);
  }

  auto author_fields = co_await authors_.TopFields(author_id, kTopFields);
  if (!author_fields) {
    co_return std::unexpected(author_fields.error());
  }
  auto community_fields = co_await communities_.TopFields(
      *run_id, (*membership)->community_id, kTopFields);
  if (!community_fields) {
    co_return std::unexpected(community_fields.error());
  }

  domain::AuthorCommunity result;
  result.author = std::move(**author);
  result.run_id = *run_id;
  result.community_id = (*membership)->community_id;
  result.community_size = (*membership)->size;
  result.author_fields = std::move(*author_fields);
  result.community_top_fields = std::move(*community_fields);
  co_return result;
}

asio::awaitable<Result<domain::CommunityDetails>> QueryService::GetCommunity(
    std::int32_t community_id, int limit, int offset) const {
  const auto run_id = co_await RequireActiveRun();
  if (!run_id) {
    co_return std::unexpected(run_id.error());
  }

  const auto size = co_await communities_.CommunitySize(*run_id, community_id);
  if (!size) {
    co_return std::unexpected(size.error());
  }
  if (!*size) {
    co_return MakeError(Errc::kNotFound, "Community {} not found",
                        community_id);
  }

  auto fields =
      co_await communities_.TopFields(*run_id, community_id, kTopFields);
  if (!fields) {
    co_return std::unexpected(fields.error());
  }
  auto members = co_await communities_.Members(
      *run_id, community_id, std::clamp(limit, 1, kMaxPageSize),
      std::max(offset, 0));
  if (!members) {
    co_return std::unexpected(members.error());
  }

  domain::CommunityDetails details;
  details.run_id = *run_id;
  details.community_id = community_id;
  details.size = **size;
  details.top_fields = std::move(*fields);
  details.members = std::move(*members);
  co_return details;
}

asio::awaitable<Result<std::vector<domain::CommunitySummary>>>
QueryService::ListCommunities(int limit) const {
  const auto run_id = co_await RequireActiveRun();
  if (!run_id) {
    co_return std::unexpected(run_id.error());
  }
  co_return co_await communities_.List(*run_id,
                                       std::clamp(limit, 1, kMaxPageSize));
}

asio::awaitable<Result<domain::GraphSlice>> QueryService::GetGraph(
    std::optional<std::int32_t> community_id, int limit_nodes,
    float min_weight) const {
  const auto run_id = co_await RequireActiveRun();
  if (!run_id) {
    co_return std::unexpected(run_id.error());
  }
  const int limit{std::clamp(limit_nodes, 1, kMaxGraphNodes)};

  // Fetch one extra row to learn whether the result was truncated.
  auto nodes = co_await communities_.TopNodes(*run_id, community_id, limit + 1);
  if (!nodes) {
    co_return std::unexpected(nodes.error());
  }

  domain::GraphSlice slice;
  slice.run_id = *run_id;
  slice.nodes = std::move(*nodes);
  if (static_cast<int>(slice.nodes.size()) > limit) {
    slice.nodes.resize(static_cast<std::size_t>(limit));
    slice.truncated = true;
  }

  std::vector<std::int64_t> ids;
  ids.reserve(slice.nodes.size());
  for (const auto& node : slice.nodes) {
    ids.push_back(node.author_id);
  }

  auto edges = co_await graph_.LoadEdgesAmong(std::move(ids),
                                              std::max(min_weight, 0.0F));
  if (!edges) {
    co_return std::unexpected(edges.error());
  }
  slice.edges = std::move(*edges);
  co_return slice;
}

}  // namespace schools::service

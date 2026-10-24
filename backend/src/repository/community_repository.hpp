#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <optional>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

class CommunityRepository {
 public:
  explicit CommunityRepository(Database& db) : db_(db) {}

  // ---- Writing a clustering run ----
  boost::asio::awaitable<Result<std::int64_t>> CreateRun(
      domain::RunParams params);

  // Multi-row INSERTs in chunks.
  boost::asio::awaitable<Status> SaveMemberships(
      std::int64_t run_id, std::vector<domain::MembershipRow> rows);

  // Fills `communities`, makes the run active (single-row pointer table, so
  // the switch is atomic) and prunes all but the 2 newest runs.
  boost::asio::awaitable<Status> ActivateRun(std::int64_t run_id,
                                             double modularity,
                                             std::int32_t community_count);

  // ---- Reading (always against a given run, normally the active one) ----
  boost::asio::awaitable<Result<std::optional<std::int64_t>>> ActiveRunId();

  boost::asio::awaitable<Result<std::optional<domain::AuthorMembership>>>
  FindMembership(std::int64_t run_id, std::int64_t author_id);

  boost::asio::awaitable<Result<std::optional<std::int32_t>>> CommunitySize(
      std::int64_t run_id, std::int32_t community_id);

  boost::asio::awaitable<Result<std::vector<domain::FieldCount>>> TopFields(
      std::int64_t run_id, std::int32_t community_id, int limit);

  boost::asio::awaitable<Result<std::vector<domain::CommunityMember>>> Members(
      std::int64_t run_id, std::int32_t community_id, int limit, int offset);

  boost::asio::awaitable<Result<std::vector<domain::CommunitySummary>>> List(
      std::int64_t run_id, int limit);

  // Highest weighted-degree authors, optionally restricted to one community.
  boost::asio::awaitable<Result<std::vector<domain::GraphNode>>> TopNodes(
      std::int64_t run_id, std::optional<std::int32_t> community_id, int limit);

 private:
  Database& db_;
};

}  // namespace schools::repository

#include "repository/community_repository.hpp"

#include <span>

#include "repository/sql_util.hpp"

namespace schools::repository {
namespace asio = boost::asio;
namespace mysql = boost::mysql;

namespace {

asio::awaitable<Status> DeleteRun(mysql::any_connection* conn,
                                  std::int64_t run_id) {
  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec1] = co_await conn->async_execute(
      mysql::with_params("DELETE FROM author_communities WHERE run_id = {}",
                         run_id),
      result, diag, kAsTuple);
  if (ec1) {
    co_return std::unexpected(DbError(ec1, diag));
  }
  const auto [ec2] = co_await conn->async_execute(
      mysql::with_params("DELETE FROM communities WHERE run_id = {}", run_id),
      result, diag, kAsTuple);
  if (ec2) {
    co_return std::unexpected(DbError(ec2, diag));
  }
  const auto [ec3] = co_await conn->async_execute(
      mysql::with_params("DELETE FROM community_runs WHERE id = {}", run_id),
      result, diag, kAsTuple);
  if (ec3) {
    co_return std::unexpected(DbError(ec3, diag));
  }
  co_return Status{};
}

}  // namespace

asio::awaitable<Result<std::int64_t>> CommunityRepository::CreateRun(
    domain::RunParams params) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params(
          "INSERT INTO community_runs (algorithm, resolution, vertices, edges) "
          "VALUES ({}, {}, {}, {})",
          params.algorithm, params.resolution, params.vertices, params.edges),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }
  co_return static_cast<std::int64_t>(result.last_insert_id());
}

asio::awaitable<Status> CommunityRepository::SaveMemberships(
    std::int64_t run_id, std::vector<domain::MembershipRow> rows) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection* conn{&lease->get()};

  const auto format_row{[run_id](const domain::MembershipRow& row,
                                 mysql::format_context_base& ctx) {
    mysql::format_sql_to(ctx, "({}, {}, {}, {})", run_id, row.author_id,
                         row.community_id, row.weighted_degree);
  }};
  co_return co_await BulkExecute<domain::MembershipRow>(
      conn,
      "INSERT INTO author_communities "
      "(run_id, author_id, community_id, weighted_degree) VALUES {}",
      std::span<const domain::MembershipRow>(rows), format_row);
}

asio::awaitable<Status> CommunityRepository::ActivateRun(
    std::int64_t run_id, double modularity, std::int32_t community_count) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;

  auto [ec1] = co_await conn.async_execute(
      mysql::with_params(
          "INSERT INTO communities (run_id, community_id, size) "
          "SELECT run_id, community_id, COUNT(*) FROM author_communities "
          "WHERE run_id = {} GROUP BY run_id, community_id",
          run_id),
      result, diag, kAsTuple);
  if (ec1) {
    co_return std::unexpected(DbError(ec1, diag));
  }

  auto [ec2] = co_await conn.async_execute(
      mysql::with_params(
          "UPDATE community_runs SET modularity = {}, communities = {} "
          "WHERE id = {}",
          modularity, community_count, run_id),
      result, diag, kAsTuple);
  if (ec2) {
    co_return std::unexpected(DbError(ec2, diag));
  }

  auto [ec3] = co_await conn.async_execute(
      mysql::with_params("INSERT INTO active_run (id, run_id) VALUES (1, {}) "
                         "ON DUPLICATE KEY UPDATE run_id = VALUES(run_id)",
                         run_id),
      result, diag, kAsTuple);
  if (ec3) {
    co_return std::unexpected(DbError(ec3, diag));
  }

  // Prune everything except the two newest runs (the new one is among them).
  auto [ec4] = co_await conn.async_execute(
      "SELECT id FROM community_runs ORDER BY id DESC "
      "LIMIT 18446744073709551615 OFFSET 2",
      result, diag, kAsTuple);
  if (ec4) {
    co_return std::unexpected(DbError(ec4, diag));
  }

  std::vector<std::int64_t> stale_runs;
  for (const mysql::row_view row : result.rows()) {
    stale_runs.push_back(AsInt(row[0]));
  }
  for (const std::int64_t stale : stale_runs) {
    auto status = co_await DeleteRun(&conn, stale);
    if (!status) {
      co_return status;
    }
  }
  co_return Status{};
}

asio::awaitable<Result<std::optional<std::int64_t>>>
CommunityRepository::ActiveRunId() {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      "SELECT run_id FROM active_run WHERE id = 1", result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }
  if (result.rows().empty()) {
    co_return std::optional<std::int64_t>{};
  }
  co_return std::optional<std::int64_t>{AsInt(result.rows()[0][0])};
}

asio::awaitable<Result<std::optional<domain::AuthorMembership>>>
CommunityRepository::FindMembership(std::int64_t run_id,
                                    std::int64_t author_id) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params(
          "SELECT ac.community_id, c.size FROM author_communities ac "
          "JOIN communities c ON c.run_id = ac.run_id "
          "AND c.community_id = ac.community_id "
          "WHERE ac.run_id = {} AND ac.author_id = {}",
          run_id, author_id),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }
  if (result.rows().empty()) {
    co_return std::optional<domain::AuthorMembership>{};
  }
  const mysql::row_view row{result.rows()[0]};
  co_return domain::AuthorMembership{static_cast<std::int32_t>(AsInt(row[0])),
                                     static_cast<std::int32_t>(AsInt(row[1]))};
}

asio::awaitable<Result<std::optional<std::int32_t>>>
CommunityRepository::CommunitySize(std::int64_t run_id,
                                   std::int32_t community_id) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params("SELECT size FROM communities WHERE run_id = {} AND "
                         "community_id = {}",
                         run_id, community_id),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }
  if (result.rows().empty()) {
    co_return std::optional<std::int32_t>{};
  }
  co_return std::optional<std::int32_t>{
      static_cast<std::int32_t>(AsInt(result.rows()[0][0]))};
}

asio::awaitable<Result<std::vector<domain::FieldCount>>>
CommunityRepository::TopFields(std::int64_t run_id, std::int32_t community_id,
                               int limit) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params(
          "SELECT f.id, f.name, CAST(SUM(af.works_count) AS SIGNED) AS cnt "
          "FROM author_communities ac "
          "JOIN author_fields af ON af.author_id = ac.author_id "
          "JOIN fields f ON f.id = af.field_id "
          "WHERE ac.run_id = {} AND ac.community_id = {} "
          "GROUP BY f.id, f.name ORDER BY cnt DESC LIMIT {}",
          run_id, community_id, limit),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  std::vector<domain::FieldCount> fields;
  for (const mysql::row_view row : result.rows()) {
    fields.push_back({static_cast<std::int32_t>(AsInt(row[0])),
                      AsString(row[1]),
                      static_cast<std::int32_t>(AsInt(row[2]))});
  }
  co_return fields;
}

asio::awaitable<Result<std::vector<domain::CommunityMember>>>
CommunityRepository::Members(std::int64_t run_id, std::int32_t community_id,
                             int limit, int offset) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params(
          "SELECT a.openalex_id, a.name, a.orcid, ac.weighted_degree "
          "FROM author_communities ac "
          "JOIN authors a ON a.openalex_id = ac.author_id "
          "WHERE ac.run_id = {} AND ac.community_id = {} "
          "ORDER BY ac.weighted_degree DESC, a.openalex_id LIMIT {} OFFSET {}",
          run_id, community_id, limit, offset),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  std::vector<domain::CommunityMember> members;
  for (const mysql::row_view row : result.rows()) {
    members.push_back({AsInt(row[0]), AsString(row[1]),
                       AsOptionalString(row[2]), AsFloat(row[3])});
  }
  co_return members;
}

asio::awaitable<Result<std::vector<domain::CommunitySummary>>>
CommunityRepository::List(std::int64_t run_id, int limit) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params(
          "SELECT community_id, size FROM communities WHERE run_id = {} "
          "ORDER BY size DESC, community_id LIMIT {}",
          run_id, limit),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  std::vector<domain::CommunitySummary> summaries;
  for (const mysql::row_view row : result.rows()) {
    summaries.push_back({static_cast<std::int32_t>(AsInt(row[0])),
                         static_cast<std::int32_t>(AsInt(row[1]))});
  }
  co_return summaries;
}

asio::awaitable<Result<std::vector<domain::GraphNode>>>
CommunityRepository::TopNodes(std::int64_t run_id,
                              std::optional<std::int32_t> community_id,
                              int limit) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  boost::system::error_code ec;
  if (community_id) {
    const auto [e] = co_await conn.async_execute(
        mysql::with_params(
            "SELECT ac.author_id, a.name, ac.community_id, ac.weighted_degree "
            "FROM author_communities ac "
            "JOIN authors a ON a.openalex_id = ac.author_id "
            "WHERE ac.run_id = {} AND ac.community_id = {} "
            "ORDER BY ac.weighted_degree DESC, ac.author_id LIMIT {}",
            run_id, *community_id, limit),
        result, diag, kAsTuple);
    ec = e;
  } else {
    const auto [e] = co_await conn.async_execute(
        mysql::with_params(
            "SELECT ac.author_id, a.name, ac.community_id, ac.weighted_degree "
            "FROM author_communities ac "
            "JOIN authors a ON a.openalex_id = ac.author_id "
            "WHERE ac.run_id = {} "
            "ORDER BY ac.weighted_degree DESC, ac.author_id LIMIT {}",
            run_id, limit),
        result, diag, kAsTuple);
    ec = e;
  }
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  std::vector<domain::GraphNode> nodes;
  for (const mysql::row_view row : result.rows()) {
    nodes.push_back({AsInt(row[0]), AsString(row[1]),
                     static_cast<std::int32_t>(AsInt(row[2])),
                     AsFloat(row[3])});
  }
  co_return nodes;
}

}  // namespace schools::repository

#include "repository/graph_repository.hpp"

#include "repository/sql_util.hpp"

namespace schools::repository {
namespace asio = boost::asio;
namespace mysql = boost::mysql;

asio::awaitable<Status> GraphRepository::RebuildDerivedTables(
    int max_ru_authors_per_work) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection* conn{&lease->get()};

  // ---- coauthor_edges ----
  for (const std::string_view sql :
       {"DROP TABLE IF EXISTS coauthor_edges_new",
        "CREATE TABLE coauthor_edges_new LIKE coauthor_edges"}) {
    auto status = co_await ExecSimple(conn, sql);
    if (!status) {
      co_return status;
    }
  }
  {
    mysql::results result;
    mysql::diagnostics diag;
    const auto [ec] = co_await conn->async_execute(
        mysql::with_params("INSERT INTO coauthor_edges_new (src, dst, weight) "
                           "SELECT a.author_id, b.author_id, COUNT(*) "
                           "FROM works w "
                           "JOIN authorships a ON a.work_id = w.openalex_id "
                           "JOIN authorships b ON b.work_id = w.openalex_id "
                           "AND b.author_id > a.author_id "
                           "WHERE w.ru_authors_count BETWEEN 2 AND {} "
                           "GROUP BY a.author_id, b.author_id",
                           max_ru_authors_per_work),
        result, diag, kAsTuple);
    if (ec) {
      co_return std::unexpected(DbError(ec, diag));
    }
  }
  for (const std::string_view sql :
       {"DROP TABLE IF EXISTS coauthor_edges_old",
        "RENAME TABLE coauthor_edges TO coauthor_edges_old, "
        "coauthor_edges_new TO coauthor_edges",
        "DROP TABLE coauthor_edges_old"}) {
    auto status = co_await ExecSimple(conn, sql);
    if (!status) {
      co_return status;
    }
  }

  // ---- author_fields ----
  for (const std::string_view sql :
       {"DROP TABLE IF EXISTS author_fields_new",
        "CREATE TABLE author_fields_new LIKE author_fields",
        "INSERT INTO author_fields_new (author_id, field_id, works_count) "
        "SELECT a.author_id, wf.field_id, COUNT(*) "
        "FROM authorships a JOIN work_fields wf ON wf.work_id = a.work_id "
        "GROUP BY a.author_id, wf.field_id",
        "DROP TABLE IF EXISTS author_fields_old",
        "RENAME TABLE author_fields TO author_fields_old, "
        "author_fields_new TO author_fields",
        "DROP TABLE author_fields_old"}) {
    auto status = co_await ExecSimple(conn, sql);
    if (!status) {
      co_return status;
    }
  }
  co_return Status{};
}

asio::awaitable<Result<std::vector<std::int64_t>>>
GraphRepository::LoadVertexIds() {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::execution_state state;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_start_execution(
      "SELECT DISTINCT author_id FROM authorships ORDER BY author_id", state,
      diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  std::vector<std::int64_t> ids;
  while (state.should_read_rows()) {
    const auto [read_ec, rows] =
        co_await conn.async_read_some_rows(state, diag, kAsTuple);
    if (read_ec) {
      co_return std::unexpected(DbError(read_ec, diag));
    }
    for (const mysql::row_view row : rows) {
      ids.push_back(AsInt(row[0]));
    }
  }
  co_return ids;
}

asio::awaitable<Status> GraphRepository::ForEachEdge(
    std::function<void(std::int64_t, std::int64_t, float)> visitor) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::execution_state state;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_start_execution(
      "SELECT src, dst, weight FROM coauthor_edges", state, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  while (state.should_read_rows()) {
    const auto [read_ec, rows] =
        co_await conn.async_read_some_rows(state, diag, kAsTuple);
    if (read_ec) {
      co_return std::unexpected(DbError(read_ec, diag));
    }
    for (const mysql::row_view row : rows) {
      visitor(AsInt(row[0]), AsInt(row[1]), AsFloat(row[2]));
    }
  }
  co_return Status{};
}

asio::awaitable<Result<std::vector<domain::WeightedEdge>>>
GraphRepository::LoadEdgesAmong(std::vector<std::int64_t> author_ids,
                                float min_weight) {
  std::vector<domain::WeightedEdge> edges;
  if (author_ids.empty()) {
    co_return edges;
  }
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  const auto format_id{[](std::int64_t id, mysql::format_context_base& ctx) {
    mysql::format_sql_to(ctx, "{}", id);
  }};
  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params("SELECT src, dst, weight FROM coauthor_edges "
                         "WHERE src IN ({}) AND dst IN ({}) AND weight >= {}",
                         mysql::sequence(author_ids, format_id),
                         mysql::sequence(author_ids, format_id), min_weight),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  edges.reserve(result.rows().size());
  for (const mysql::row_view row : result.rows()) {
    edges.push_back({AsInt(row[0]), AsInt(row[1]), AsFloat(row[2])});
  }
  co_return edges;
}

}  // namespace schools::repository

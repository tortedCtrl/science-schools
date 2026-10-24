#include "repository/ingest_repository.hpp"

#include <cstdint>
#include <span>
#include <vector>

#include "repository/sql_util.hpp"

namespace schools::repository {
namespace asio = boost::asio;
namespace mysql = boost::mysql;

namespace {

struct IdPair {
  std::int64_t first{0};
  std::int64_t second{0};
};

asio::awaitable<Status> WritePage(mysql::any_connection* conn,
                                  const std::string* key,
                                  const domain::WorksPage* page,
                                  const PartitionState* next) {
  const auto format_author{
      [](const domain::Author& author, mysql::format_context_base& ctx) {
        const mysql::field_view orcid{
            author.orcid ? mysql::field_view(std::string_view(*author.orcid))
                         : mysql::field_view()};
        mysql::format_sql_to(ctx, "({}, {}, {})", author.openalex_id, orcid,
                             author.name);
      }};
  auto status = co_await BulkExecute<domain::Author>(
      conn,
      "INSERT INTO authors (openalex_id, orcid, name) VALUES {} "
      "ON DUPLICATE KEY UPDATE orcid = COALESCE(VALUES(orcid), orcid), "
      "name = VALUES(name)",
      std::span<const domain::Author>(page->authors), format_author);
  if (!status) {
    co_return status;
  }

  const auto format_field{
      [](const domain::Field& field, mysql::format_context_base& ctx) {
        mysql::format_sql_to(ctx, "({}, {})", field.id, field.name);
      }};
  status = co_await BulkExecute<domain::Field>(
      conn,
      "INSERT INTO fields (id, name) VALUES {} "
      "ON DUPLICATE KEY UPDATE name = VALUES(name)",
      std::span<const domain::Field>(page->fields), format_field);
  if (!status) {
    co_return status;
  }

  const auto format_work{[](const domain::Work& work,
                            mysql::format_context_base& ctx) {
    mysql::format_sql_to(ctx, "({}, {}, {})", work.openalex_id,
                         work.publication_year,
                         static_cast<std::int32_t>(work.ru_author_ids.size()));
  }};
  status = co_await BulkExecute<domain::Work>(
      conn,
      "INSERT INTO works (openalex_id, publication_year, ru_authors_count) "
      "VALUES {} ON DUPLICATE KEY UPDATE "
      "publication_year = VALUES(publication_year), "
      "ru_authors_count = VALUES(ru_authors_count)",
      std::span<const domain::Work>(page->works), format_work);
  if (!status) {
    co_return status;
  }

  std::vector<IdPair> authorships;
  std::vector<IdPair> work_fields;
  for (const domain::Work& work : page->works) {
    for (const std::int64_t author_id : work.ru_author_ids) {
      authorships.push_back({work.openalex_id, author_id});
    }
    for (const std::int32_t field_id : work.field_ids) {
      work_fields.push_back({work.openalex_id, field_id});
    }
  }
  const auto format_pair{
      [](const IdPair& pair, mysql::format_context_base& ctx) {
        mysql::format_sql_to(ctx, "({}, {})", pair.first, pair.second);
      }};
  status = co_await BulkExecute<IdPair>(
      conn, "INSERT IGNORE INTO authorships (work_id, author_id) VALUES {}",
      std::span<const IdPair>(authorships), format_pair);
  if (!status) {
    co_return status;
  }
  status = co_await BulkExecute<IdPair>(
      conn, "INSERT IGNORE INTO work_fields (work_id, field_id) VALUES {}",
      std::span<const IdPair>(work_fields), format_pair);
  if (!status) {
    co_return status;
  }

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn->async_execute(
      mysql::with_params(
          "INSERT INTO ingest_partitions "
          "(part_key, cursor_value, done, works_loaded) "
          "VALUES ({}, {}, {}, {}) ON DUPLICATE KEY UPDATE "
          "cursor_value = VALUES(cursor_value), done = VALUES(done), "
          "works_loaded = works_loaded + VALUES(works_loaded)",
          *key, next->cursor, next->done ? 1 : 0,
          static_cast<std::int64_t>(page->works.size())),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }
  co_return Status{};
}

}  // namespace

asio::awaitable<Result<std::optional<PartitionState>>>
IngestRepository::LoadState(std::string key) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params("SELECT cursor_value, done FROM ingest_partitions "
                         "WHERE part_key = {}",
                         key),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }

  if (result.rows().empty()) {
    co_return std::optional<PartitionState>{};
  }
  const mysql::row_view row{result.rows()[0]};
  co_return PartitionState{AsString(row[0]), AsInt(row[1]) != 0};
}

asio::awaitable<Status> IngestRepository::SavePage(std::string key,
                                                   domain::WorksPage page,
                                                   PartitionState next) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection* conn{&lease->get()};

  auto status = co_await ExecSimple(conn, "START TRANSACTION");
  if (!status) {
    co_return status;
  }

  status = co_await WritePage(conn, &key, &page, &next);
  if (!status) {
    // Best effort; the pool also resets the connection when it is returned.
    (void)co_await ExecSimple(conn, "ROLLBACK");
    co_return status;
  }
  co_return co_await ExecSimple(conn, "COMMIT");
}

asio::awaitable<Status> IngestRepository::ResetPartition(std::string key) {
  auto lease = co_await db_.Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  mysql::any_connection& conn{lease->get()};

  mysql::results result;
  mysql::diagnostics diag;
  const auto [ec] = co_await conn.async_execute(
      mysql::with_params("DELETE FROM ingest_partitions WHERE part_key = {}",
                         key),
      result, diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }
  co_return Status{};
}

}  // namespace schools::repository

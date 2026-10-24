#include "repository/schema.hpp"

#include <spdlog/spdlog.h>

#include <array>
#include <string_view>

#include "repository/sql_util.hpp"

namespace schools::repository {
namespace {

// OpenAlex ids are stored as BIGINT (the numeric part of "A123", "W456"),
// which is far smaller and faster to join than the URL/string form.
// No foreign keys: they only slow down bulk loading, and the application
// controls the write order.
constexpr auto kStatements = std::to_array<std::string_view>({
    R"sql(CREATE TABLE IF NOT EXISTS authors (
      openalex_id BIGINT NOT NULL PRIMARY KEY,
      orcid       VARCHAR(32) NULL,
      name        VARCHAR(1024) NOT NULL,
      INDEX authors_orcid_idx (orcid)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    R"sql(CREATE TABLE IF NOT EXISTS fields (
      id   INT NOT NULL PRIMARY KEY,
      name VARCHAR(255) NOT NULL
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    R"sql(CREATE TABLE IF NOT EXISTS works (
      openalex_id      BIGINT NOT NULL PRIMARY KEY,
      publication_year SMALLINT NULL,
      ru_authors_count INT NOT NULL,
      INDEX works_year_idx (publication_year)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    // Only authorships with an RU affiliation are stored.
    R"sql(CREATE TABLE IF NOT EXISTS authorships (
      work_id   BIGINT NOT NULL,
      author_id BIGINT NOT NULL,
      PRIMARY KEY (work_id, author_id),
      INDEX authorships_author_idx (author_id)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    R"sql(CREATE TABLE IF NOT EXISTS work_fields (
      work_id  BIGINT NOT NULL,
      field_id INT NOT NULL,
      PRIMARY KEY (work_id, field_id),
      INDEX work_fields_field_idx (field_id)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    // Resume support: one row per ingestion partition (month).
    R"sql(CREATE TABLE IF NOT EXISTS ingest_partitions (
      part_key     VARCHAR(64) NOT NULL PRIMARY KEY,
      cursor_value TEXT NOT NULL,
      done         TINYINT(1) NOT NULL DEFAULT 0,
      works_loaded BIGINT NOT NULL DEFAULT 0,
      updated_at   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP
                   ON UPDATE CURRENT_TIMESTAMP
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    // Derived tables, rebuilt by the clustering step.
    R"sql(CREATE TABLE IF NOT EXISTS author_fields (
      author_id   BIGINT NOT NULL,
      field_id    INT NOT NULL,
      works_count INT NOT NULL,
      PRIMARY KEY (author_id, field_id)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    R"sql(CREATE TABLE IF NOT EXISTS coauthor_edges (
      src    BIGINT NOT NULL,
      dst    BIGINT NOT NULL,
      weight FLOAT NOT NULL,
      PRIMARY KEY (src, dst),
      INDEX coauthor_edges_dst_idx (dst)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    R"sql(CREATE TABLE IF NOT EXISTS community_runs (
      id          BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
      created_at  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
      algorithm   VARCHAR(32) NOT NULL,
      resolution  DOUBLE NOT NULL,
      vertices    BIGINT NOT NULL,
      edges       BIGINT NOT NULL,
      communities INT NULL,
      modularity  DOUBLE NULL
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    // Single row (id = 1) pointing to the run served by the API.
    R"sql(CREATE TABLE IF NOT EXISTS active_run (
      id     TINYINT NOT NULL PRIMARY KEY,
      run_id BIGINT NOT NULL
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    R"sql(CREATE TABLE IF NOT EXISTS author_communities (
      run_id          BIGINT NOT NULL,
      author_id       BIGINT NOT NULL,
      community_id    INT NOT NULL,
      weighted_degree FLOAT NOT NULL,
      PRIMARY KEY (run_id, author_id),
      INDEX author_communities_community_idx (run_id, community_id, weighted_degree DESC),
      INDEX author_communities_degree_idx (run_id, weighted_degree DESC)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",

    R"sql(CREATE TABLE IF NOT EXISTS communities (
      run_id       BIGINT NOT NULL,
      community_id INT NOT NULL,
      size         INT NOT NULL,
      PRIMARY KEY (run_id, community_id),
      INDEX communities_size_idx (run_id, size DESC)
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4)sql",
});

}  // namespace

boost::asio::awaitable<Status> Migrate(Database* db) {
  auto lease = co_await db->Acquire();
  if (!lease) {
    co_return std::unexpected(lease.error());
  }
  boost::mysql::any_connection* conn{&lease->get()};
  for (const std::string_view statement : kStatements) {
    auto status = co_await ExecSimple(conn, statement);
    if (!status) {
      co_return status;
    }
  }
  spdlog::info("Database schema is up to date");
  co_return Status{};
}

}  // namespace schools::repository

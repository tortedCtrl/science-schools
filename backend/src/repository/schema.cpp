#include "repository/schema.hpp"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

namespace schools::repository {
namespace {

constexpr auto kStatements = std::to_array<std::string_view>({
    R"sql(
      CREATE TABLE IF NOT EXISTS authors (
        id INTEGER PRIMARY KEY,
        openalex_id VARCHAR(64) NOT NULL UNIQUE,
        name VARCHAR(512) NOT NULL,
        institution_name VARCHAR(512)
      )
    )sql",
    R"sql(
      CREATE TABLE IF NOT EXISTS coauthorship_edges (
        source_author_id INTEGER NOT NULL REFERENCES authors(id),
        target_author_id INTEGER NOT NULL REFERENCES authors(id),
        weight INTEGER NOT NULL,
        CHECK (source_author_id < target_author_id),
        UNIQUE (source_author_id, target_author_id)
      )
    )sql",
    R"sql(
      CREATE TABLE IF NOT EXISTS author_meta_articles (
        id VARCHAR(64) PRIMARY KEY,
        title VARCHAR(512) NOT NULL,
        year INTEGER
      )
    )sql",
    R"sql(
      CREATE TABLE IF NOT EXISTS author_to_article (
        author_id INTEGER NOT NULL REFERENCES authors(id),
        article_id VARCHAR(64) NOT NULL REFERENCES author_meta_articles(id),
        PRIMARY KEY (author_id, article_id)
      )
    )sql",
    R"sql(
      CREATE TABLE IF NOT EXISTS author_communities (
        author_id INTEGER PRIMARY KEY REFERENCES authors(id) ON DELETE CASCADE,
        community_id INTEGER NOT NULL
      )
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS coauthorship_edges_target_idx
        ON coauthorship_edges (target_author_id)
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS author_to_article_article_idx
        ON author_to_article (article_id)
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS author_communities_community_idx
        ON author_communities (community_id)
    )sql",
});

void MigrateLegacyCommunityColumn(soci::session& sql) {
  std::int32_t legacy_columns{0};
  sql << "SELECT COUNT(*) FROM information_schema.columns "
         "WHERE table_schema = current_schema() "
         "AND table_name = 'authors' AND column_name = 'community_id'",
      soci::into(legacy_columns);
  if (legacy_columns == 0) {
    return;
  }

  sql << "INSERT INTO author_communities (author_id, community_id) "
         "SELECT id, community_id FROM authors WHERE community_id IS NOT NULL "
         "ON CONFLICT (author_id) DO NOTHING";
  sql << "ALTER TABLE authors DROP COLUMN community_id";
  spdlog::info("Migrated authors.community_id to author_communities");
}

}  // namespace

Status EnsureSchema(Database& database) {
  auto session = database.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
    for (const std::string_view statement : kStatements) {
      sql << std::string(statement);
    }
    MigrateLegacyCommunityColumn(sql);
    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "ensure database schema: {}",
                     error.what());
  }
}

}  // namespace schools::repository

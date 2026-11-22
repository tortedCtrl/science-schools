#include "repository/schema.hpp"

#include <soci/soci.h>

#include <array>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>

namespace schools::repository {
namespace {

bool HasTable(soci::session& sql, std::string_view table) {
  std::int32_t count{0};
  const std::string table_name{table};
  sql << "SELECT COUNT(*) FROM information_schema.tables "
         "WHERE table_schema = current_schema() AND table_name = :table_name",
      soci::use(table_name), soci::into(count);
  return count != 0;
}

bool HasColumn(soci::session& sql, std::string_view table,
               std::string_view column) {
  std::int32_t count{0};
  const std::string table_name{table};
  const std::string column_name{column};
  sql << "SELECT COUNT(*) FROM information_schema.columns "
         "WHERE table_schema = current_schema() AND table_name = :table_name "
         "AND column_name = :column_name",
      soci::use(table_name), soci::use(column_name), soci::into(count);
  return count != 0;
}

void RenameLegacyTable(soci::session& sql, std::string_view table,
                       std::string_view legacy_table,
                       std::string_view required_column) {
  if (!HasTable(sql, table) || HasColumn(sql, table, required_column)) {
    return;
  }
  if (HasTable(sql, legacy_table)) {
    throw std::runtime_error("both legacy and current tables exist for " +
                             std::string(table));
  }
  sql << "ALTER TABLE " + std::string(table) + " RENAME TO " +
      std::string(legacy_table);
}

std::int64_t InsertRun(soci::session& sql, std::string_view algorithm) {
  std::int64_t run_id{0};
  const std::string algorithm_name{algorithm};
  sql << "INSERT INTO clustering_runs (algorithm) VALUES (:algorithm) "
         "RETURNING id",
      soci::use(algorithm_name), soci::into(run_id);
  return run_id;
}

void MigrateLegacyResults(soci::session& sql) {
  const bool has_legacy_leiden =
      HasTable(sql, "author_communities_legacy");
  const bool has_legacy_bigclam =
      HasTable(sql, "bigclam_memberships_legacy");
  const bool has_legacy_topics = HasTable(sql, "community_topics_legacy");

  bool migrate_leiden = has_legacy_leiden;
  bool migrate_bigclam = has_legacy_bigclam;
  if (has_legacy_topics) {
    std::int32_t count{0};
    sql << "SELECT COUNT(*) FROM community_topics_legacy "
           "WHERE algorithm = 'leiden'",
        soci::into(count);
    migrate_leiden = migrate_leiden || count != 0;
    sql << "SELECT COUNT(*) FROM community_topics_legacy "
           "WHERE algorithm = 'bigclam'",
        soci::into(count);
    migrate_bigclam = migrate_bigclam || count != 0;
  }

  std::int64_t leiden_run_id{0};
  if (migrate_leiden) {
    leiden_run_id = InsertRun(sql, "leiden");
    if (has_legacy_leiden) {
      sql << "INSERT INTO author_communities (run_id, author_id, community_id) "
             "SELECT :run_id, author_id, community_id "
             "FROM author_communities_legacy ON CONFLICT DO NOTHING",
          soci::use(leiden_run_id);
    }
    if (has_legacy_topics) {
      sql << "INSERT INTO community_topics "
             "(run_id, community_id, topic_id, topic_name, weight, "
             "article_count) "
             "SELECT :run_id, community_id, topic_id, topic_name, weight, "
             "article_count FROM community_topics_legacy "
             "WHERE algorithm = 'leiden' ON CONFLICT DO NOTHING",
          soci::use(leiden_run_id);
    }
  }

  std::int64_t bigclam_run_id{0};
  if (migrate_bigclam) {
    bigclam_run_id = InsertRun(sql, "bigclam");
    if (has_legacy_bigclam) {
      sql << "INSERT INTO author_communities (run_id, author_id, community_id) "
             "SELECT :run_id, author_id, community_id "
             "FROM bigclam_memberships_legacy ON CONFLICT DO NOTHING",
          soci::use(bigclam_run_id);
    }
    if (has_legacy_topics) {
      sql << "INSERT INTO community_topics "
             "(run_id, community_id, topic_id, topic_name, weight, "
             "article_count) "
             "SELECT :run_id, community_id, topic_id, topic_name, weight, "
             "article_count FROM community_topics_legacy "
             "WHERE algorithm = 'bigclam' ON CONFLICT DO NOTHING",
          soci::use(bigclam_run_id);
    }
  }

  if (has_legacy_leiden) {
    sql << "DROP TABLE author_communities_legacy";
  }
  if (has_legacy_bigclam) {
    sql << "DROP TABLE bigclam_memberships_legacy";
  }
  if (has_legacy_topics) {
    sql << "DROP TABLE community_topics_legacy";
  }
}

void MigrateLegacyCommunityColumn(soci::session& sql) {
  if (!HasColumn(sql, "authors", "community_id")) {
    return;
  }

  std::int32_t count{0};
  sql << "SELECT COUNT(*) FROM authors WHERE community_id IS NOT NULL",
      soci::into(count);
  if (count != 0) {
    const std::int64_t run_id = InsertRun(sql, "leiden");
    sql << "INSERT INTO author_communities (run_id, author_id, community_id) "
           "SELECT :run_id, id, community_id FROM authors "
           "WHERE community_id IS NOT NULL ON CONFLICT DO NOTHING",
        soci::use(run_id);
  }
  sql << "ALTER TABLE authors DROP COLUMN community_id";
}

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
      CREATE TABLE IF NOT EXISTS article_topics (
        article_id TEXT NOT NULL
          REFERENCES author_meta_articles(id) ON DELETE CASCADE,
        topic_id TEXT NOT NULL,
        topic_name TEXT NOT NULL,
        score DOUBLE PRECISION NOT NULL,
        is_primary BOOLEAN NOT NULL DEFAULT FALSE,
        subfield_id TEXT NOT NULL DEFAULT '',
        subfield_name TEXT NOT NULL DEFAULT '',
        field_id TEXT NOT NULL DEFAULT '',
        field_name TEXT NOT NULL DEFAULT '',
        domain_id TEXT NOT NULL DEFAULT '',
        domain_name TEXT NOT NULL DEFAULT '',
        PRIMARY KEY (article_id, topic_id)
      )
    )sql",
    R"sql(
      CREATE TABLE IF NOT EXISTS clustering_runs (
        id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
        algorithm TEXT NOT NULL CHECK (algorithm IN ('leiden', 'bigclam')),
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
      )
    )sql",
    R"sql(
      CREATE TABLE IF NOT EXISTS author_communities (
        run_id BIGINT NOT NULL REFERENCES clustering_runs(id) ON DELETE CASCADE,
        author_id INTEGER NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
        community_id INTEGER NOT NULL,
        PRIMARY KEY (run_id, author_id, community_id)
      )
    )sql",
    R"sql(
      CREATE TABLE IF NOT EXISTS community_topics (
        run_id BIGINT NOT NULL REFERENCES clustering_runs(id) ON DELETE CASCADE,
        community_id INTEGER NOT NULL,
        topic_id TEXT NOT NULL,
        topic_name TEXT NOT NULL,
        weight DOUBLE PRECISION NOT NULL,
        article_count INTEGER NOT NULL,
        PRIMARY KEY (run_id, community_id, topic_id)
      )
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS idx_article_topics_topic_id
        ON article_topics (topic_id)
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS idx_author_to_article_article_id
        ON author_to_article (article_id)
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS idx_author_communities_author_run
        ON author_communities (author_id, run_id)
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS idx_author_communities_run_community
        ON author_communities (run_id, community_id)
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS idx_community_topics_run_weight
        ON community_topics (run_id, community_id, weight DESC)
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS idx_coauthorship_edges_target
        ON coauthorship_edges (target_author_id)
    )sql",
    R"sql(
      CREATE INDEX IF NOT EXISTS idx_clustering_runs_algorithm_created
        ON clustering_runs (algorithm, id DESC)
    )sql",
    R"sql(
      CREATE OR REPLACE VIEW article_coauthorship_edges AS
      SELECT ata1.article_id,
             ata1.author_id AS source_author_id,
             ata2.author_id AS target_author_id,
             art.year,
             at.topic_id,
             at.topic_name,
             at.score AS topic_score
      FROM author_to_article ata1
      JOIN author_to_article ata2
        ON ata2.article_id = ata1.article_id
       AND ata1.author_id < ata2.author_id
      JOIN author_meta_articles art ON art.id = ata1.article_id
      LEFT JOIN article_topics at ON at.article_id = ata1.article_id
    )sql",
});

}  // namespace

Status EnsureSchema(Database& database) {
  auto session = database.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);

    RenameLegacyTable(sql, "author_communities", "author_communities_legacy",
                      "run_id");
    RenameLegacyTable(sql, "bigclam_memberships", "bigclam_memberships_legacy",
                      "run_id");
    RenameLegacyTable(sql, "community_topics", "community_topics_legacy",
                      "run_id");

    for (const std::string_view statement : kStatements) {
      sql << std::string(statement);
    }
    MigrateLegacyResults(sql);
    MigrateLegacyCommunityColumn(sql);
    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "ensure database schema: {}",
                     error.what());
  }
}

}  // namespace schools::repository

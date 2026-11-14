#include "repository/author_repository.hpp"

#include <soci/soci.h>

#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace schools::repository {
namespace {

constexpr std::string_view kWeightedDegreeCte = R"sql(
  WITH weighted_degrees AS (
    SELECT author_id, SUM(weight) AS weighted_degree
    FROM (
      SELECT source_author_id AS author_id, weight FROM coauthorship_edges
      UNION ALL
      SELECT target_author_id AS author_id, weight FROM coauthorship_edges
    ) AS incident_edges
    GROUP BY author_id
  )
)sql";

domain::AuthorSummary ReadAuthorSummary(const soci::row& row) {
  domain::AuthorSummary author{};
  author.id = row.get<std::int32_t>(0);
  author.openalex_id = row.get<std::string>(1);
  author.name = row.get<std::string>(2);
  if (row.get_indicator(3) != soci::i_null) {
    author.community_id = row.get<std::int32_t>(3);
  }
  if (row.get_indicator(4) != soci::i_null) {
    author.institution_name = row.get<std::string>(4);
  }
  author.weighted_degree = row.get<std::int64_t>(5);
  return author;
}

}  // namespace

Result<std::vector<domain::AuthorSummary>> AuthorRepository::ListAuthors() {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const std::string query = std::string(kWeightedDegreeCte) + R"sql(
      SELECT a.id, a.openalex_id, a.name, ac.community_id,
             a.institution_name, COALESCE(wd.weighted_degree, 0)
      FROM authors a
      LEFT JOIN author_communities ac ON ac.author_id = a.id
      LEFT JOIN weighted_degrees wd ON wd.author_id = a.id
      ORDER BY a.id
    )sql";

    const soci::rowset<soci::row> rows = (sql.prepare << query);
    std::vector<domain::AuthorSummary> authors;
    for (const soci::row& row : rows) {
      authors.push_back(ReadAuthorSummary(row));
    }
    return authors;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list authors: {}", error.what());
  }
}

Result<std::optional<domain::AuthorCard>> AuthorRepository::GetAuthorCard(
    std::int32_t author_id) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const std::string query = std::string(kWeightedDegreeCte) + R"sql(
      SELECT a.id, a.openalex_id, a.name, ac.community_id,
             a.institution_name, COALESCE(wd.weighted_degree, 0),
             art.id, art.title, art.year
      FROM authors a
      LEFT JOIN author_communities ac ON ac.author_id = a.id
      LEFT JOIN weighted_degrees wd ON wd.author_id = a.id
      LEFT JOIN author_to_article ata ON ata.author_id = a.id
      LEFT JOIN author_meta_articles art ON art.id = ata.article_id
      WHERE a.id = :author_id
      ORDER BY art.year DESC NULLS LAST
      LIMIT 5
    )sql";

    const soci::rowset<soci::row> rows =
        (sql.prepare << query, soci::use(author_id));

    domain::AuthorCard card{};
    bool found = false;
    for (const soci::row& row : rows) {
      if (!found) {
        card.author = ReadAuthorSummary(row);
        found = true;
      }
      if (row.get_indicator(6) == soci::i_null) {
        continue;
      }
      domain::Article article{};
      article.openalex_id = row.get<std::string>(6);
      article.title = row.get<std::string>(7);
      if (row.get_indicator(8) != soci::i_null) {
        article.year = row.get<std::int32_t>(8);
      }
      card.articles.push_back(std::move(article));
    }

    if (!found) {
      return std::optional<domain::AuthorCard>{};
    }
    return card;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "get author card {}: {}", author_id,
                     error.what());
  }
}

}  // namespace schools::repository

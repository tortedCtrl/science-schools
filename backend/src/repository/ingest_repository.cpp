#include "repository/ingest_repository.hpp"

#include <soci/soci.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <string>
#include <vector>

#include "repository/bulk_insert.hpp"

namespace schools::repository {
namespace {

void InsertAuthors(soci::session& sql,
                   std::span<const domain::Author> authors) {
  std::vector<std::int32_t> ids;
  std::vector<std::string> openalex_ids;
  std::vector<std::string> names;
  std::vector<std::string> institutions;
  ids.reserve(authors.size());
  openalex_ids.reserve(authors.size());
  names.reserve(authors.size());
  institutions.reserve(authors.size());

  std::int32_t id{0};
  for (const domain::Author& author : authors) {
    ids.push_back(id);
    ++id;
    openalex_ids.push_back(author.openalex_id);
    names.push_back(author.name);
    institutions.push_back(author.institution_name);
  }

  for (std::size_t offset = 0; offset < ids.size(); offset += kBulkChunkSize) {
    const std::size_t end = std::min(offset + kBulkChunkSize, ids.size());
    auto id_chunk = Slice(ids, offset, end);
    auto openalex_chunk = Slice(openalex_ids, offset, end);
    auto name_chunk = Slice(names, offset, end);
    auto institution_chunk = Slice(institutions, offset, end);
    soci::statement statement =
        (sql.prepare << "INSERT INTO authors "
                        "(id, openalex_id, name, institution_name) "
                        "VALUES (:id, :openalex_id, :name, :institution_name) "
                        "ON CONFLICT DO NOTHING",
         soci::use(id_chunk), soci::use(openalex_chunk), soci::use(name_chunk),
         soci::use(institution_chunk));
    statement.execute(true);
  }
}

void InsertEdges(soci::session& sql, std::span<const domain::Edge> edges) {
  std::vector<std::int32_t> sources;
  std::vector<std::int32_t> targets;
  std::vector<std::int32_t> weights;
  sources.reserve(edges.size());
  targets.reserve(edges.size());
  weights.reserve(edges.size());
  for (const domain::Edge& edge : edges) {
    sources.push_back(edge.source);
    targets.push_back(edge.target);
    weights.push_back(edge.weight);
  }

  for (std::size_t offset = 0; offset < sources.size();
       offset += kBulkChunkSize) {
    const std::size_t end = std::min(offset + kBulkChunkSize, sources.size());
    auto source_chunk = Slice(sources, offset, end);
    auto target_chunk = Slice(targets, offset, end);
    auto weight_chunk = Slice(weights, offset, end);
    soci::statement statement =
        (sql.prepare << "INSERT INTO coauthorship_edges "
                        "(source_author_id, target_author_id, weight) "
                        "VALUES (:source, :target, :weight) "
                        "ON CONFLICT DO NOTHING",
         soci::use(source_chunk), soci::use(target_chunk),
         soci::use(weight_chunk));
    statement.execute(true);
  }
}

void InsertArticles(soci::session& sql,
                    std::span<const domain::Article> articles) {
  std::vector<std::string> ids;
  std::vector<std::string> titles;
  std::vector<std::int32_t> years;
  ids.reserve(articles.size());
  titles.reserve(articles.size());
  years.reserve(articles.size());
  for (const domain::Article& article : articles) {
    ids.push_back(article.openalex_id);
    titles.push_back(article.title);
    years.push_back(article.year);
  }

  for (std::size_t offset = 0; offset < ids.size(); offset += kBulkChunkSize) {
    const std::size_t end = std::min(offset + kBulkChunkSize, ids.size());
    auto id_chunk = Slice(ids, offset, end);
    auto title_chunk = Slice(titles, offset, end);
    auto year_chunk = Slice(years, offset, end);
    soci::statement statement =
        (sql.prepare << "INSERT INTO author_meta_articles (id, title, year) "
                        "VALUES (:id, :title, :year) "
                        "ON CONFLICT DO NOTHING",
         soci::use(id_chunk), soci::use(title_chunk), soci::use(year_chunk));
    statement.execute(true);
  }
}

void InsertAuthorArticles(soci::session& sql,
                          std::span<const domain::AuthorArticle> links) {
  std::vector<std::int32_t> author_ids;
  std::vector<std::string> article_ids;
  author_ids.reserve(links.size());
  article_ids.reserve(links.size());
  for (const domain::AuthorArticle& link : links) {
    author_ids.push_back(link.author_id);
    article_ids.push_back(link.article_id);
  }

  for (std::size_t offset = 0; offset < author_ids.size();
       offset += kBulkChunkSize) {
    const std::size_t end =
        std::min(offset + kBulkChunkSize, author_ids.size());
    auto author_chunk = Slice(author_ids, offset, end);
    auto article_chunk = Slice(article_ids, offset, end);
    soci::statement statement =
        (sql.prepare << "INSERT INTO author_to_article (author_id, article_id) "
                        "VALUES (:author_id, :article_id) "
                        "ON CONFLICT DO NOTHING",
         soci::use(author_chunk), soci::use(article_chunk));
    statement.execute(true);
  }
}

}  // namespace

Status IngestRepository::ClearImportData() {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
    sql << "DELETE FROM author_communities";
    sql << "DELETE FROM author_to_article";
    sql << "DELETE FROM coauthorship_edges";
    sql << "DELETE FROM author_meta_articles";
    sql << "DELETE FROM authors";
    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "clear imported graph: {}", error.what());
  }
}

Status IngestRepository::SaveGraph(
    std::span<const domain::Author> authors,
    std::span<const domain::Edge> edges,
    std::span<const domain::Article> articles,
    std::span<const domain::AuthorArticle> links) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
    InsertAuthors(sql, authors);
    InsertEdges(sql, edges);
    InsertArticles(sql, articles);
    InsertAuthorArticles(sql, links);
    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "save imported graph: {}", error.what());
  }
}

}  // namespace schools::repository

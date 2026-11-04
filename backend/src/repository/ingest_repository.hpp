#pragma once

#include <span>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

class IngestRepository {
 public:
  explicit IngestRepository(Database& database) : database_(database) {}

  // Wipes the imported graph. Communities are a separate table owned by the
  // clustering step, so they are cleared with the rest of the imported graph.
  Status ClearImportData();

  // Writes authors, edges, articles and author-to-article links in one
  // transaction. Each table is written with multi-row INSERTs, not per row.
  Status SaveGraph(std::span<const domain::Author> authors,
                   std::span<const domain::Edge> edges,
                   std::span<const domain::Article> articles,
                   std::span<const domain::AuthorArticle> links);

 private:
  Database& database_;
};

}  // namespace schools::repository

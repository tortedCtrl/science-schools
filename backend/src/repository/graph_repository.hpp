#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <functional>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

class GraphRepository {
 public:
  explicit GraphRepository(Database& db) : db_(db) {}

  // Rebuilds coauthor_edges and author_fields from authorships entirely inside
  // the database (set-based, no data crosses the wire). Each table is built
  // under a new name and swapped in with an atomic RENAME TABLE, so readers
  // never see a half-built graph.
  // Edge weight = number of works two authors wrote together. Works with more
  // than `max_ru_authors_per_work` RU authors are skipped: one 1000-author
  // paper would otherwise create ~500k edges.
  boost::asio::awaitable<Status> RebuildDerivedTables(
      int max_ru_authors_per_work);

  // All distinct RU author ids, sorted (index in this vector = graph vertex).
  boost::asio::awaitable<Result<std::vector<std::int64_t>>> LoadVertexIds();

  // Streams all edges (src < dst) in batches without materializing the whole
  // result set in the driver.
  boost::asio::awaitable<Status> ForEachEdge(
      std::function<void(std::int64_t, std::int64_t, float)> visitor);

  // Edges whose both endpoints are in `author_ids`.
  boost::asio::awaitable<Result<std::vector<domain::WeightedEdge>>>
  LoadEdgesAmong(std::vector<std::int64_t> author_ids, float min_weight);

 private:
  Database& db_;
};

}  // namespace schools::repository

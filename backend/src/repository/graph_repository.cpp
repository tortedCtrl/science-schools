#include "repository/graph_repository.hpp"

#include <soci/soci.h>

#include <cstdint>
#include <exception>
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

domain::Edge ReadEdge(const soci::row& row) {
  domain::Edge edge{};
  edge.source = row.get<std::int32_t>(0);
  edge.target = row.get<std::int32_t>(1);
  edge.weight = row.get<std::int32_t>(2);
  return edge;
}

domain::AuthorSummary ReadNode(const soci::row& row) {
  domain::AuthorSummary author{};
  author.id = row.get<std::int32_t>(0);
  author.name = row.get<std::string>(1);
  if (row.get_indicator(2) != soci::i_null) {
    author.community_id = row.get<std::int32_t>(2);
  }
  if (row.get_indicator(3) != soci::i_null) {
    author.institution_name = row.get<std::string>(3);
  }
  author.weighted_degree = row.get<std::int64_t>(4);
  return author;
}

}  // namespace

Result<std::vector<domain::Edge>> GraphRepository::ListEdges() {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const soci::rowset<soci::row> rows =
        (sql.prepare << "SELECT source_author_id, target_author_id, weight "
                        "FROM coauthorship_edges "
                        "ORDER BY source_author_id, target_author_id");
    std::vector<domain::Edge> edges;
    for (const soci::row& row : rows) {
      edges.push_back(ReadEdge(row));
    }
    return edges;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list edges: {}", error.what());
  }
}

Result<domain::GraphData> GraphRepository::GetGraph() {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const std::string node_query = std::string(kWeightedDegreeCte) + R"sql(
      SELECT a.id, a.name, ac.community_id, a.institution_name,
             COALESCE(wd.weighted_degree, 0)
      FROM authors a
      LEFT JOIN author_communities ac ON ac.author_id = a.id
      LEFT JOIN weighted_degrees wd ON wd.author_id = a.id
      ORDER BY a.id
    )sql";

    domain::GraphData graph{};
    const soci::rowset<soci::row> nodes = (sql.prepare << node_query);
    for (const soci::row& row : nodes) {
      graph.nodes.push_back(ReadNode(row));
    }

    const soci::rowset<soci::row> edges =
        (sql.prepare << "SELECT source_author_id, target_author_id, weight "
                        "FROM coauthorship_edges "
                        "ORDER BY source_author_id, target_author_id");
    for (const soci::row& row : edges) {
      graph.links.push_back(ReadEdge(row));
    }
    return graph;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "load graph: {}", error.what());
  }
}

}  // namespace schools::repository

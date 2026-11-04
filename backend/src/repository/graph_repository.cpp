#include "repository/graph_repository.hpp"

#include <soci/soci.h>

#include <cstdint>
#include <exception>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
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

Result<std::vector<domain::ArticleTopicInput>>
GraphRepository::GetArticleTopicInputs() {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const soci::rowset<soci::row> rows =
        (sql.prepare << "SELECT art.id, art.title, COALESCE(art.year, 0), "
                        "ata.author_id, topic.topic_id, topic.topic_name, "
                        "topic.score "
                        "FROM author_meta_articles art "
                        "JOIN author_to_article ata ON ata.article_id = art.id "
                        "JOIN article_topics topic ON topic.article_id = art.id "
                        "ORDER BY art.id, ata.author_id, topic.topic_id");
    std::map<std::string, domain::ArticleTopicInput> inputs_by_article;
    std::map<std::string, std::unordered_set<std::int32_t>> authors_by_article;
    std::map<std::string, std::unordered_set<std::string>> topics_by_article;

    for (const soci::row& row : rows) {
      const std::string article_id = row.get<std::string>(0);
      auto [input_it, inserted] = inputs_by_article.try_emplace(article_id);
      domain::ArticleTopicInput& input = input_it->second;
      if (inserted) {
        input.article.openalex_id = article_id;
        input.article.title = row.get<std::string>(1);
        input.article.year = row.get<std::int32_t>(2);
      }

      const std::int32_t author_id = row.get<std::int32_t>(3);
      if (authors_by_article[article_id].insert(author_id).second) {
        input.author_ids.push_back(author_id);
      }

      const std::string topic_id = row.get<std::string>(4);
      if (topics_by_article[article_id].insert(topic_id).second) {
        input.article.topics.push_back(
            {.topic_id = topic_id,
             .name = row.get<std::string>(5),
             .score = row.get<double>(6)});
      }
    }

    std::vector<domain::ArticleTopicInput> inputs;
    inputs.reserve(inputs_by_article.size());
    for (auto& [article_id, input] : inputs_by_article) {
      (void)article_id;
      inputs.push_back(std::move(input));
    }
    return inputs;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "load article topic inputs: {}",
                     error.what());
  }
}

}  // namespace schools::repository

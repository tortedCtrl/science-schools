#include "service/query_service.hpp"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

#include "domain/models.hpp"

namespace schools::service {
namespace {

using Json = nlohmann::json;

Json AuthorJson(const domain::AuthorSummary& author) {
  Json value;
  value["id"] = author.id;
  value["openalex_id"] = author.openalex_id;
  value["name"] = author.name;
  value["community_id"] =
      author.community_id ? Json(*author.community_id) : Json(nullptr);
  value["institution_name"] =
      author.institution_name ? Json(*author.institution_name) : Json(nullptr);
  value["weighted_degree"] = author.weighted_degree;
  return value;
}

Json ArticleJson(const domain::Article& article) {
  Json value;
  value["id"] = article.openalex_id;
  value["title"] = article.title;
  value["year"] = article.year;
  return value;
}

Json EdgeJson(const domain::Edge& edge) {
  Json value;
  value["source"] = std::to_string(edge.source);
  value["target"] = std::to_string(edge.target);
  value["weight"] = edge.weight;
  return value;
}

}  // namespace

Result<std::string> QueryService::ListAuthorsJson() {
  auto authors = authors_.ListAuthors();
  if (!authors) {
    return std::unexpected(authors.error());
  }

  Json result = Json::array();
  for (const domain::AuthorSummary& author : *authors) {
    result.push_back(AuthorJson(author));
  }
  return result.dump();
}

Result<std::string> QueryService::GetAuthorCardJson(std::int32_t author_id) {
  auto card = authors_.GetAuthorCard(author_id);
  if (!card) {
    return std::unexpected(card.error());
  }
  const auto& optional_card = *card;
  if (!optional_card.has_value()) {
    Json empty;
    empty["articles"] = Json::array();
    return empty.dump();
  }

  const domain::AuthorCard& value = optional_card.value();
  Json result;
  result["openalex_id"] = value.author.openalex_id;
  result["name"] = value.author.name;
  result["community_id"] = value.author.community_id
                               ? Json(*value.author.community_id)
                               : Json(nullptr);
  result["institution_name"] = value.author.institution_name
                                   ? Json(*value.author.institution_name)
                                   : Json(nullptr);
  result["weighted_degree"] = value.author.weighted_degree;
  result["articles"] = Json::array();
  for (const domain::Article& article : value.articles) {
    result["articles"].push_back(ArticleJson(article));
  }
  return result.dump();
}

Result<std::string> QueryService::ListEdgesJson() {
  auto edges = graph_.ListEdges();
  if (!edges) {
    return std::unexpected(edges.error());
  }

  Json result = Json::array();
  for (const domain::Edge& edge : *edges) {
    result.push_back(EdgeJson(edge));
  }
  return result.dump();
}

Result<std::string> QueryService::GetGraphJson() {
  auto graph = graph_.GetGraph();
  if (!graph) {
    return std::unexpected(graph.error());
  }

  Json nodes = Json::array();
  for (const domain::AuthorSummary& author : graph->nodes) {
    Json node;
    node["id"] = std::to_string(author.id);
    node["name"] = author.name;
    node["community_id"] =
        author.community_id ? Json(*author.community_id) : Json(nullptr);
    node["institution_name"] = author.institution_name
                                   ? Json(*author.institution_name)
                                   : Json(nullptr);
    node["weighted_degree"] = author.weighted_degree;
    nodes.push_back(std::move(node));
  }

  Json links = Json::array();
  for (const domain::Edge& edge : graph->links) {
    links.push_back(EdgeJson(edge));
  }

  Json result;
  result["nodes"] = std::move(nodes);
  result["links"] = std::move(links);
  return result.dump();
}

}  // namespace schools::service

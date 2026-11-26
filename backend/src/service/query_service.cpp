#include "service/query_service.hpp"
#include "graph_engine/neighborhood_bfs.hpp"
#include <algorithm>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "domain/models.hpp"

namespace schools::service {
namespace {

using Json = nlohmann::json;

std::string_view AlgorithmName(domain::CommunityAlgorithm algorithm) {
  switch (algorithm) {
    case domain::CommunityAlgorithm::kLeiden:
      return "leiden";
    case domain::CommunityAlgorithm::kBigClam:
      return "bigclam";
  }
  return "leiden";
}

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

Json GraphNodeJson(const domain::GraphNodeDto& node) {
  Json value;
  value["id"] = node.id;
  value["name"] = node.name;
  value["community_id"] =
      node.community_id ? Json(*node.community_id) : Json(nullptr);
  value["communities"] = Json::array();
  for (const domain::CommunityMembershipDto& membership : node.communities) {
    value["communities"].push_back(
        {{"community_id", membership.community_id}});
  }
  value["institution_name"] =
      node.institution_name ? Json(*node.institution_name) : Json(nullptr);
  value["weighted_degree"] = node.weighted_degree;
  return value;
}

Json GraphLinkJson(const domain::GraphLinkDto& link) {
  Json value;
  value["source"] = link.source;
  value["target"] = link.target;
  value["weight"] = link.weight;
  return value;
}

Json GraphJson(const domain::GraphResponseDto& graph) {
  Json value;
  value["algorithm"] = AlgorithmName(graph.algorithm);
  value["nodes"] = Json::array();
  for (const domain::GraphNodeDto& node : graph.nodes) {
    value["nodes"].push_back(GraphNodeJson(node));
  }
  value["links"] = Json::array();
  for (const domain::GraphLinkDto& link : graph.links) {
    value["links"].push_back(GraphLinkJson(link));
  }
  return value;
}

Json ArticleSummaryJson(const domain::ArticleSummaryDto& article) {
  Json value;
  value["id"] = article.id;
  value["title"] = article.title;
  value["year"] = article.year;
  return value;
}

Json AuthorCardJson(const domain::AuthorCardResponseDto& card) {
  Json value;
  value["openalex_id"] = card.openalex_id;
  value["name"] = card.name;
  value["community_id"] =
      card.community_id ? Json(*card.community_id) : Json(nullptr);
  value["institution_name"] =
      card.institution_name ? Json(*card.institution_name) : Json(nullptr);
  value["weighted_degree"] = card.weighted_degree;
  value["articles"] = Json::array();
  for (const domain::ArticleSummaryDto& article : card.articles) {
    value["articles"].push_back(ArticleSummaryJson(article));
  }
  return value;
}

Json CommunityTopicJson(const domain::CommunityTopicDto& topic) {
  Json value;
  value["topic_id"] = topic.topic_id;
  value["name"] = topic.name;
  value["weight"] = topic.weight;
  value["article_count"] = topic.article_count;
  return value;
}

Json CommunitySummaryJson(const domain::CommunitySummary& community) {
  Json value;
  value["community_id"] = community.community_id;
  value["member_count"] = community.member_count;
  return value;
}

Json TopicInfoJson(const domain::TopicInfo& topic) {
  Json value;
  value["topic_id"] = topic.topic_id;
  value["topic_name"] = topic.topic_name;
  value["article_count"] = topic.article_count;
  return value;
}

Json ClusterMetadataJson(const domain::ClusterMetadata& cluster) {
  Json value;
  value["community_id"] = cluster.community_id;
  value["member_count"] = cluster.member_count;
  return value;
}

Json AuthorInClusterJson(const domain::AuthorInCluster& author) {
  Json value;
  value["author_id"] = author.author_id;
  value["openalex_id"] = author.openalex_id;
  value["name"] = author.name;
  return value;
}

Json ClusterByTopicJson(const domain::ClusterByTopic& cluster) {
  Json value;
  value["community_id"] = cluster.community_id;
  value["member_count"] = cluster.member_count;
  value["authors"] = Json::array();

  for (const domain::AuthorInCluster& author : cluster.authors) {
    value["authors"].push_back(AuthorInClusterJson(author));
  }

  return value;
}

Json ClusterContentJson(const domain::ClusterContent& cluster) {
  Json value;
  value["community_id"] = cluster.community_id;
  value["member_count"] = cluster.member_count;

  value["authors"] = Json::array();
  for (const domain::AuthorInCluster& author : cluster.authors) {
    value["authors"].push_back(AuthorInClusterJson(author));
  }

  value["topics"] = Json::array();
  for (const domain::TopicInfo& topic : cluster.topics) {
    value["topics"].push_back(TopicInfoJson(topic));
  }

  value["links"] = Json::array();
  for (const domain::Edge& link : cluster.links) {
    value["links"].push_back({
        {"source", link.source},
        {"target", link.target},
        {"weight", link.weight},
    });
  }

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


Result<std::string> QueryService::SearchAuthorsJson(
    std::string_view query
) {
    if (query.empty() || query.size() > 200) {
        return MakeError(
            Errc::kInvalidArgument,
            "author search must contain 1 to 200 bytes"
        );
    }

    auto authors = authors_.SearchAuthors(query);

    if (!authors) {
        return std::unexpected(authors.error());
    }

    Json result = Json::array();

    for (const domain::AuthorSummary& author : *authors) {
        result.push_back({
            {"id", author.id},
            {"name", author.name},
            {"openalex_id", author.openalex_id},
            {"institution_name", author.institution_name
                ? Json(*author.institution_name)
                : Json(nullptr)}
        });
    }

    return result.dump();
}


Result<std::string> QueryService::GetAuthorCardJson(std::int32_t author_id)
{
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
  domain::AuthorCardResponseDto response{
      .openalex_id = value.author.openalex_id,
      .name = value.author.name,
      .community_id = value.author.community_id,
      .institution_name = value.author.institution_name,
      .weighted_degree = value.author.weighted_degree,
      .articles = {}};
  response.articles.reserve(value.articles.size());
  for (const domain::Article& article : value.articles) {
    response.articles.push_back({.id = article.openalex_id,
                                 .title = article.title,
                                 .year = article.year});
  }
  return AuthorCardJson(response).dump();
}

Result<std::string> QueryService::ListEdgesJson() {
  auto edges = graph_.ListEdges();
  if (!edges) {
    return std::unexpected(edges.error());
  }

  Json result = Json::array();
  for (const domain::Edge& edge : *edges) {
    result.push_back(GraphLinkJson({.source = std::to_string(edge.source),
                                    .target = std::to_string(edge.target),
                                    .weight = edge.weight}));
  }
  return result.dump();
}

std::string QueryService::SerializeGraph(const domain::GraphData& graph_data, std::string_view algorithm) {
  std::unordered_map<std::int32_t,
                     std::vector<domain::CommunityMembershipDto>>
      memberships_by_author;
  for (const domain::Membership& membership : graph_data.memberships) {
    memberships_by_author[membership.author_id].push_back(
        {.community_id = membership.community_id});
  }

  domain::GraphResponseDto response{
                                    .algorithm =
                                        algorithm == "bigclam"
                                            ? domain::CommunityAlgorithm::kBigClam
                                            : domain::CommunityAlgorithm::kLeiden,
                                    .nodes = {},
                                    .links = {}};
  response.nodes.reserve(graph_data.nodes.size());
  for (const domain::AuthorSummary& author : graph_data.nodes) {
    auto memberships = memberships_by_author.find(author.id);
    response.nodes.push_back(
        {.id = std::to_string(author.id),
         .name = author.name,
         .community_id = author.community_id,
         .communities = memberships == memberships_by_author.end()
                            ? std::vector<domain::CommunityMembershipDto>{}
                            : memberships->second,
         .institution_name = author.institution_name,
         .weighted_degree = author.weighted_degree});
  }

  response.links.reserve(graph_data.links.size());
  for (const domain::Edge& edge : graph_data.links) {
    response.links.push_back({.source = std::to_string(edge.source),
                              .target = std::to_string(edge.target),
                              .weight = edge.weight});
  }
  return GraphJson(response).dump();
}

Result<std::string> QueryService::GetGraphJson(std::string_view algorithm) {
  auto full = graph_.GetGraph(algorithm);
  if (!full) {
    return std::unexpected(full.error());
  }
  return SerializeGraph(*full, algorithm);
}

Result<std::string> QueryService::ListCommunitiesJson(
    std::string_view algorithm) {
  auto communities = communities_.ListCommunities(algorithm);
  if (!communities) {
    return std::unexpected(communities.error());
  }

  Json result = Json::array();
  for (const domain::CommunitySummary& community : *communities) {
    result.push_back(CommunitySummaryJson(community));
  }
  return result.dump();
}

Result<std::string> QueryService::ListCommunityTopicsJson(
    std::string_view algorithm, std::int32_t community_id) {
  auto topics = communities_.ListCommunityTopics(algorithm, community_id);
  if (!topics) {
    return std::unexpected(topics.error());
  }

  Json result = Json::array();
  for (const domain::CommunityTopic& topic : *topics) {
    result.push_back(CommunityTopicJson({.topic_id = topic.topic_id,
                                         .name = topic.topic_name,
                                         .weight = topic.weight,
                                         .article_count = topic.article_count}));
  }
  return result.dump();
}

Result<std::string> QueryService::ListAllTopicsJson() {
  auto topics = communities_.ListAllTopics();
  if (!topics) {
    return std::unexpected(topics.error());
  }

  Json result = Json::array();
  for (const domain::TopicInfo& topic : *topics) {
    result.push_back(TopicInfoJson(topic));
  }

  return result.dump();
}

Result<std::string> QueryService::ListClusterMetadataJson(
    std::string_view algorithm) {
  auto clusters = communities_.ListClusterMetadata(algorithm);
  if (!clusters) {
    return std::unexpected(clusters.error());
  }

  Json result = Json::array();
  for (const domain::ClusterMetadata& cluster : *clusters) {
    result.push_back(ClusterMetadataJson(cluster));
  }

  return result.dump();
}

Result<std::string> QueryService::ListClustersByTopicJson(
    std::string_view algorithm, std::string_view topic_id) {
  auto clusters = communities_.ListClustersByTopic(algorithm, topic_id);
  if (!clusters) {
    return std::unexpected(clusters.error());
  }

  Json result = Json::array();
  for (const domain::ClusterByTopic& cluster : *clusters) {
    result.push_back(ClusterByTopicJson(cluster));
  }

  return result.dump();
}

Result<std::string> QueryService::GetClusterContentJson(
    std::string_view algorithm, std::int32_t community_id) {
  auto cluster = communities_.GetClusterContent(algorithm, community_id);
  if (!cluster) {
    return std::unexpected(cluster.error());
  }

  return ClusterContentJson(*cluster).dump();
}


Result<std::string> QueryService::GetNeighborhoodJson(
    std::string_view algorithm,
    std::int32_t author_id,
    int depth,
    std::size_t v_cap
) {
    if ((algorithm != "leiden" &&
         algorithm != "bigclam") ||
        author_id < 0 ||
        depth < 0 || depth > 3 ||
        v_cap == 0 || v_cap > 1000) {

        return MakeError(
            Errc::kInvalidArgument,
            "invalid neighborhood parameters"
        );
    }

    auto full = graph_.GetGraph(algorithm);

    if (!full) {
        return std::unexpected(full.error());
    }

    const bool exists = std::any_of(
        full->nodes.begin(),
        full->nodes.end(),
        [author_id](const auto& node) {
            return node.id == author_id;
        }
    );

    if (!exists) {
        return MakeError(
            Errc::kNotFound,
            "author {} not found",
            author_id
        );
    }

    auto result = graph::NeighborhoodBfs(
        *full, author_id, depth, v_cap
    );

    return SerializeGraph(result, algorithm);
}


}  // namespace schools::service

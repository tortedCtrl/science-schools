#include "service/community_topic_aggregator.hpp"

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace schools::service {
namespace {

constexpr double kNonMemberMembership = 0.01;

const char* AlgorithmName(domain::CommunityAlgorithm algorithm) {
  switch (algorithm) {
    case domain::CommunityAlgorithm::kLeiden:
      return "leiden";
    case domain::CommunityAlgorithm::kBigClam:
      return "bigclam";
  }
  return "unknown";
}

}  // namespace

std::vector<domain::CommunityTopic> ComputeCommunityTopics(
    std::span<const domain::Membership> memberships,
    std::span<const domain::ArticleTopicInput> articles,
    domain::CommunityAlgorithm algorithm) {
  std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>>
      communities_by_author;
  for (const domain::Membership& membership : memberships) {
    communities_by_author[membership.author_id].insert(membership.community_id);
  }

  struct Aggregate {
    double weight{0.0};
    std::int32_t article_count{0};
    std::string topic_name;
  };
  using TopicKey = std::pair<std::int32_t, std::string>;
  std::map<TopicKey, Aggregate> aggregates;

  for (const domain::ArticleTopicInput& input : articles) {
    std::unordered_set<std::int32_t> article_communities;
    for (const std::int32_t author_id : input.author_ids) {
      const auto author_communities = communities_by_author.find(author_id);
      if (author_communities != communities_by_author.end()) {
        article_communities.insert(author_communities->second.begin(),
                                   author_communities->second.end());
      }
    }

    std::unordered_set<std::string> seen_topics;
    for (const domain::ArticleTopic& topic : input.article.topics) {
      if (topic.topic_id.empty() || !seen_topics.insert(topic.topic_id).second) {
        continue;
      }

      for (const std::int32_t community_id : article_communities) {
        double article_membership = 1.0;
        for (const std::int32_t author_id : input.author_ids) {
          const auto author_communities = communities_by_author.find(author_id);
          if (author_communities == communities_by_author.end() ||
              !author_communities->second.contains(community_id)) {
            article_membership *= kNonMemberMembership;
          }
        }

        auto [aggregate_it, inserted] = aggregates.try_emplace(
            std::make_pair(community_id, topic.topic_id));
        Aggregate& aggregate = aggregate_it->second;
        if (inserted) {
          aggregate.topic_name = topic.name;
        }
        aggregate.weight += article_membership * topic.score;
        ++aggregate.article_count;
      }
    }
  }

  std::vector<domain::CommunityTopic> result;
  result.reserve(aggregates.size());
  for (const auto& [key, aggregate] : aggregates) {
    result.push_back({.algorithm = AlgorithmName(algorithm),
                      .community_id = key.first,
                      .topic_id = key.second,
                      .topic_name = aggregate.topic_name,
                      .weight = aggregate.weight,
                      .article_count = aggregate.article_count});
  }
  return result;
}

}  // namespace schools::service

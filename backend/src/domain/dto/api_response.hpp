#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/common/types.hpp"
#include "domain/common/enums.hpp"

namespace schools::domain {

struct CommunityMembershipDto {
  CommunityId community_id{0};
};

struct GraphNodeDto {
  std::string id;
  std::string name;
  std::optional<CommunityId> community_id;
  std::vector<CommunityMembershipDto> communities;
  std::optional<std::string> institution_name;
  Weight weighted_degree{0};
};

struct GraphLinkDto {
  std::string source;
  std::string target;
  Weight weight{0};
};

struct GraphResponseDto {
  CommunityAlgorithm algorithm{CommunityAlgorithm::kLeiden};
  std::vector<GraphNodeDto> nodes;
  std::vector<GraphLinkDto> links;
};

struct ArticleSummaryDto {
  std::string id;
  std::string title;
  std::int32_t year{0};
};

struct AuthorCardResponseDto {
  std::string openalex_id;
  std::string name;
  std::optional<CommunityId> community_id;
  std::optional<std::string> institution_name;
  Weight weighted_degree{0};
  std::vector<ArticleSummaryDto> articles;
};

struct CommunityTopicDto {
  std::string topic_id;
  std::string name;
  double weight{0.0};
  std::int32_t article_count{0};
};

}  // namespace schools::domain

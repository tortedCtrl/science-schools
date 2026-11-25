#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

class CommunityRepository {
 public:
  explicit CommunityRepository(Database& database) : database_{database} {}

  Result<std::int64_t> SaveLeidenResults(
      std::span<const domain::Membership> memberships,
      std::span<const domain::CommunityTopic> topics);

  Result<std::vector<domain::CommunitySummary>> ListCommunities(
      std::string_view algorithm);

  Result<std::vector<domain::CommunityTopic>> ListCommunityTopics(
      std::string_view algorithm, std::int32_t community_id);

  Result<std::vector<domain::TopicInfo>> ListAllTopics();

  Result<std::vector<domain::ClusterMetadata>> ListClusterMetadata(
      std::string_view algorithm);

  Result<std::vector<domain::ClusterByTopic>> ListClustersByTopic(
      std::string_view algorithm, std::string_view topic_id);

  Result<domain::ClusterContent> GetClusterContent(
      std::string_view algorithm, std::int32_t community_id);

  Result<std::int64_t> SaveBigClamResults(
      std::span<const domain::Membership> memberships,
      std::span<const domain::CommunityTopic> topics);

 private:
  Database& database_;
};

}  // namespace schools::repository


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

  Result<std::vector<domain::CommunityTopic>> ListTopCommunityTopics(
      std::string_view algorithm);

  Result<std::vector<domain::Membership>> ListBigClamMemberships();

  Result<std::int64_t> SaveBigClamResults(
      std::span<const domain::Membership> memberships,
      std::span<const domain::CommunityTopic> topics);

 private:
  Database& database_;
};

}  // namespace schools::repository

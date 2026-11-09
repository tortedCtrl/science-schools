#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

// Stores the output of the Leiden run separately from author rows. The
// clustering algorithm owns this table; author data remains in `authors`.
class CommunityRepository {
 public:
  explicit CommunityRepository(Database& database) : database_{database} {}

  Status ReplaceLeidenResults(
      std::span<const domain::Membership> memberships,
      std::span<const domain::CommunityTopic> topics);

  Result<std::vector<domain::CommunityTopic>> ListTopCommunityTopics(
      std::string_view algorithm);

  Result<std::vector<domain::Membership>> ListBigClamMemberships();

  Status ReplaceBigClamResults(
      std::span<const domain::Membership> memberships,
      std::span<const domain::CommunityTopic> topics);

 private:
  Database& database_;
};

}  // namespace schools::repository

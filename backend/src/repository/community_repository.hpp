#pragma once

#include <span>

#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

// Stores the output of the Leiden run separately from author rows. The
// clustering algorithm owns this table; author data remains in `authors`.
class CommunityRepository {
 public:
  explicit CommunityRepository(Database& database) : database_{database} {}

  Status ReplaceMemberships(std::span<const domain::Membership> memberships);

 private:
  Database& database_;
};

}  // namespace schools::repository

#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

class AuthorRepository {
 public:
  explicit AuthorRepository(Database& database) : database_(database) {}

  Result<std::vector<domain::AuthorSummary>> ListAuthors();
  Result<std::optional<domain::AuthorCard>> GetAuthorCard(
      std::int32_t author_id);

 private:
  Database& database_;
};

}  // namespace schools::repository

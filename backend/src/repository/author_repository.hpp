#pragma once

#include <boost/asio.hpp>
#include <cstdint>
#include <optional>
#include <vector>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

class AuthorRepository {
 public:
  explicit AuthorRepository(Database& db) : db_(db) {}

  boost::asio::awaitable<Result<std::optional<domain::Author>>> Find(
      std::int64_t openalex_id);

  // Research areas of an author, most frequent first (uses author_fields).
  boost::asio::awaitable<Result<std::vector<domain::FieldCount>>> TopFields(
      std::int64_t openalex_id, int limit);

 private:
  Database& db_;
};

}  // namespace schools::repository

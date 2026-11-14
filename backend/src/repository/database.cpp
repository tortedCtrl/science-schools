#include "repository/database.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <string>

namespace schools::repository {

Status Database::Open(std::string_view connection_string, int pool_size) {
  if (pool_.has_value()) {
    return MakeError(Errc::kInternal, "database is already open");
  }

  const std::size_t size = static_cast<std::size_t>(std::max(pool_size, 1));
  try {
    pool_.emplace(size);
    for (std::size_t index = 0; index < size; ++index) {
      pool_->at(index).open(soci::postgresql, std::string(connection_string));
    }
    return {};
  } catch (const std::exception& error) {
    pool_.reset();
    return MakeError(Errc::kDatabase, "open PostgreSQL connection pool: {}",
                     error.what());
  }
}

Result<std::unique_ptr<soci::session>> Database::Acquire() {
  if (!pool_.has_value()) {
    return MakeError(Errc::kDatabase, "database is not open");
  }

  try {
    return std::make_unique<soci::session>(*pool_);
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "acquire PostgreSQL connection: {}",
                     error.what());
  }
}

}  // namespace schools::repository

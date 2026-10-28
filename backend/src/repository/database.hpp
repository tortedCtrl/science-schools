#pragma once

#include <soci/postgresql/soci-postgresql.h>
#include <soci/soci.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

#include "common/error.hpp"

namespace schools::repository {

// Thread-safe SOCI connection pool. Repositories lease a session for the
// duration of one operation; httplib handles requests on many threads, so a
// single shared soci::session would be a data race.
class Database {
 public:
  Database() = default;
  ~Database() = default;

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) = delete;
  Database& operator=(Database&&) = delete;

  Status Open(std::string_view connection_string, int pool_size);

  Result<std::unique_ptr<soci::session>> Acquire();

 private:
  std::optional<soci::connection_pool> pool_;
};

}  // namespace schools::repository

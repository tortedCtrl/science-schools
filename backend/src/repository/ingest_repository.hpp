#pragma once

#include <boost/asio.hpp>
#include <optional>
#include <string>

#include "common/error.hpp"
#include "domain/models.hpp"
#include "repository/database.hpp"

namespace schools::repository {

struct PartitionState {
  std::string cursor{"*"};  // cursor for the NEXT page to fetch
  bool done{false};
};

class IngestRepository {
 public:
  explicit IngestRepository(Database& db) : db_(db) {}

  boost::asio::awaitable<Result<std::optional<PartitionState>>> LoadState(
      std::string key);

  // Saves a whole page in ONE transaction using multi-row INSERTs (never row
  // by row): authors, fields, works, authorships, work_fields and the
  // partition cursor. Atomic: after a crash the stored cursor always matches
  // the stored data.
  boost::asio::awaitable<Status> SavePage(std::string key,
                                          domain::WorksPage page,
                                          PartitionState next);

  boost::asio::awaitable<Status> ResetPartition(std::string key);

 private:
  Database& db_;
};

}  // namespace schools::repository

#pragma once

#include "repository/database.hpp"

namespace schools::repository {

// Creates the tables used by the API and the ingestion job. It is idempotent
// and makes local runs work without docker-entrypoint-initdb.d.
Status EnsureSchema(Database& database);

}  // namespace schools::repository

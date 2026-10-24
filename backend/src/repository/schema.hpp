#pragma once

#include <boost/asio.hpp>

#include "common/error.hpp"
#include "repository/database.hpp"

namespace schools::repository {

// Creates all tables and indexes if they do not exist yet (idempotent).
boost::asio::awaitable<Status> Migrate(Database* db);

}  // namespace schools::repository

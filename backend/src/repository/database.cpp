#include "repository/database.hpp"

#include <utility>

#include "common/async.hpp"
#include "repository/sql_util.hpp"

namespace schools::repository {
namespace {

boost::mysql::pool_params MakePoolParams(const Config& config) {
  boost::mysql::pool_params params;
  params.server_address.emplace_host_and_port(config.db_host, config.db_port);
  params.username = config.db_user;
  params.password = config.db_password;
  params.database = config.db_name;
  params.max_size = config.db_pool_size;
  // The pool is shared by all coroutines running on the io_context, which is
  // driven by several threads.
  params.thread_safe = true;
  return params;
}

}  // namespace

Database::Database(boost::asio::io_context& ioc, const Config& config)
    : pool_{ioc.get_executor(), MakePoolParams(config)} {}

void Database::Start() { pool_.async_run(boost::asio::detached); }

void Database::Cancel() { pool_.cancel(); }

boost::asio::awaitable<Result<boost::mysql::pooled_connection>>
Database::Acquire() {
  boost::mysql::diagnostics diag;
  auto [ec, connection] = co_await pool_.async_get_connection(diag, kAsTuple);
  if (ec) {
    co_return std::unexpected(DbError(ec, diag));
  }
  co_return std::move(connection);
}

}  // namespace schools::repository

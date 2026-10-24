#pragma once

#include <boost/asio.hpp>
#include <boost/mysql.hpp>

#include "common/config.hpp"
#include "common/error.hpp"

namespace schools::repository {

// Thin facade over boost::mysql::connection_pool. The pool is asynchronous and
// thread-safe, so there are no mutexes, condition variables or blocking calls
// in our code: a coroutine that needs the database simply co_awaits a
// connection and is resumed when one is free.
class Database {
 public:
  Database(boost::asio::io_context& ioc, const Config& config);

  // Starts the pool's background connection management. Call once before
  // io_context::run().
  void Start();

  // Stops the pool; pending and future Acquire() calls fail.
  void Cancel();

  // Returns a connection to the pool when the returned object is destroyed.
  boost::asio::awaitable<Result<boost::mysql::pooled_connection>> Acquire();

 private:
  boost::mysql::connection_pool pool_;
};

}  // namespace schools::repository

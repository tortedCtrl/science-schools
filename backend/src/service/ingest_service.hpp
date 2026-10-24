#pragma once

#include <boost/asio.hpp>

#include "common/config.hpp"
#include "common/error.hpp"
#include "repository/ingest_repository.hpp"

namespace schools::service {

struct IngestOptions {
  int from_year{2010};
  int to_year{2026};
  bool force{false};  // ignore saved cursors and re-read every partition
};

// Downloads works from OpenAlex and stores them.
//
// Parallelism: a cursor stream cannot be parallelized, so the date range is
// split into monthly partitions; `ingest_concurrency` coroutines each pull the
// next partition and read it page by page over their own keep-alive HTTPS
// connection (Boost.Asio + Beast). A shared lock-free token bucket keeps the
// overall request rate under the OpenAlex limit. Database access is natively
// asynchronous (Boost.MySQL pool), so nothing here blocks an I/O thread.
class IngestService {
 public:
  IngestService(const Config& config, repository::IngestRepository& repository)
      : config_{config}, repository_{repository} {}

  // Completes when every partition is finished. Partition failures are logged
  // and make the final status an error; they are resumed on the next run from
  // the stored cursor.
  boost::asio::awaitable<Status> Run(IngestOptions options);

 private:
  const Config& config_;
  repository::IngestRepository& repository_;
};

}  // namespace schools::service

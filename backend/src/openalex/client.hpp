#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <string>

#include "common/error.hpp"
#include "common/rate_limiter.hpp"
#include "domain/models.hpp"
#include "openalex/https_session.hpp"

namespace schools::openalex {

struct ClientOptions {
  std::string host{"api.openalex.org"};
  std::string mailto;
  std::string api_key;
  int max_retries{5};
};

// Which slice of /works to read. Slices (months) are independent cursor
// streams, which is what makes parallel ingestion possible.
struct WorksQuery {
  std::string country_code;
  std::string from_date;  // YYYY-MM-DD
  std::string to_date;    // YYYY-MM-DD
  int page_size{200};
};

// One client = one keep-alive connection. Create one per concurrent worker.
class OpenAlexClient {
 public:
  OpenAlexClient(boost::asio::any_io_executor executor,
                 boost::asio::ssl::context& ssl_context, ClientOptions options,
                 std::shared_ptr<RateLimiter> limiter);

  // Fetches one page. `cursor` is "*" for the first page.
  // Retries on 429 / 5xx / network errors with exponential backoff.
  boost::asio::awaitable<Result<domain::WorksPage>> FetchWorksPage(
      WorksQuery query, std::string cursor);

 private:
  ClientOptions options_;
  std::shared_ptr<RateLimiter> limiter_;
  HttpsSession session_;
};

}  // namespace schools::openalex

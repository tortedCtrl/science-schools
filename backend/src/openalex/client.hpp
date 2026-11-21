#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "common/error.hpp"
#include "common/rate_limiter.hpp"
#include "domain/models.hpp"

namespace schools::openalex {

struct ClientOptions {
  std::string base_url{"https://api.openalex.org"};
  std::string mailto;
  std::string api_key;
  int max_retries{5};
};

// Thin, thread-safe OpenAlex client. Each thread uses its own reusable libcurl
// easy handle, so parallel workers also reuse their HTTPS connections.
class OpenAlexClient {
 public:
  struct Response {
    std::int64_t status{0};
    std::string body;
    std::string retry_after;
  };

  OpenAlexClient(ClientOptions options, RateLimiter& limiter);

  Result<std::vector<domain::Author>> FetchAuthorsPage(int page, int per_page);

  Result<std::vector<domain::Work>> FetchWorksPage(
      std::span<const std::string> author_ids,
      const std::unordered_set<std::string>& selected_authors, int page,
      int per_page);

 private:
  [[nodiscard]] Status Get(
      std::string_view url,
      const std::function<Status(std::string_view)>& consume_response);
  [[nodiscard]] Result<Response> Perform(std::string_view url) const;

  ClientOptions options_;
  RateLimiter& limiter_;
};

}  // namespace schools::openalex

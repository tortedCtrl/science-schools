#include "openalex/client.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <string_view>
#include <utility>

#include "common/async.hpp"
#include "common/parse.hpp"
#include "common/url.hpp"
#include "openalex/parser.hpp"

namespace schools::openalex {
namespace asio = boost::asio;
namespace http = boost::beast::http;

namespace {

std::string BuildTarget(const ClientOptions& options, const WorksQuery& query,
                        std::string_view cursor) {
  std::string target{
      fmt::format("/works?filter=authorships.institutions.country_code:{},"
                  "from_publication_date:{},to_publication_date:{}"
                  "&select=id,publication_year,authorships,topics"
                  "&per-page={}&cursor={}",
                  query.country_code, query.from_date, query.to_date,
                  query.page_size, UrlEncode(cursor))};
  if (!options.mailto.empty()) {
    target += "&mailto=" + UrlEncode(options.mailto);
  }
  if (!options.api_key.empty()) {
    target += "&api_key=" + UrlEncode(options.api_key);
  }
  return target;
}

bool IsRetryable(http::status status) {
  return status == http::status::too_many_requests ||
         static_cast<unsigned>(status) >= 500;
}

std::chrono::milliseconds BackoffDelay(int attempt) {
  const int capped{std::min(attempt, 6)};
  return std::chrono::milliseconds(500LL << capped);  // 0.5s .. 32s
}

asio::awaitable<void> SleepFor(std::chrono::milliseconds delay) {
  asio::steady_timer timer(co_await asio::this_coro::executor);
  timer.expires_after(delay);
  const auto [ec] = co_await timer.async_wait(kAsTuple);
  (void)ec;
}

std::string UserAgent(const ClientOptions& options) {
  return options.mailto.empty()
             ? "scientific-schools/0.1"
             : fmt::format("scientific-schools/0.1 (mailto:{})",
                           options.mailto);
}

}  // namespace

OpenAlexClient::OpenAlexClient(asio::any_io_executor executor,
                               asio::ssl::context& ssl_context,
                               ClientOptions options,
                               std::shared_ptr<RateLimiter> limiter)
    : options_{std::move(options)},
      limiter_{std::move(limiter)},
      session_{std::move(executor), ssl_context, options_.host,
               UserAgent(options_)} {}

asio::awaitable<Result<domain::WorksPage>> OpenAlexClient::FetchWorksPage(
    WorksQuery query, std::string cursor) {
  const std::string target{BuildTarget(options_, query, cursor)};

  for (int attempt = 0;; ++attempt) {
    co_await limiter_->Acquire();

    std::string failure;
    bool retryable{true};
    std::chrono::milliseconds delay{BackoffDelay(attempt)};

    const auto response = co_await session_.Get(target);
    if (response) {
      if (response->result() == http::status::ok) {
        co_return ParseWorksPage(response->body(), query.country_code);
      }
      failure =
          fmt::format("HTTP {}", static_cast<unsigned>(response->result()));
      retryable = IsRetryable(response->result());
      if (const auto it = response->find(http::field::retry_after);
          it != response->end()) {
        const std::string_view value(it->value().data(), it->value().size());
        if (const auto seconds = ParseNumber<int>(value, "Retry-After")) {
          delay = std::chrono::seconds(*seconds);
        }
      }
    } else {
      failure = response.error().message;
    }

    if (!retryable || attempt >= options_.max_retries) {
      co_return MakeError(Errc::kNetwork,
                          "OpenAlex request failed after {} attempt(s): {}",
                          attempt + 1, failure);
    }
    spdlog::warn("OpenAlex request failed ({}), retry {} in {} ms", failure,
                 attempt + 1, delay.count());
    co_await SleepFor(delay);
  }
}

}  // namespace schools::openalex

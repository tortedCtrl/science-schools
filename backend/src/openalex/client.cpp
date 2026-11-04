#include "openalex/client.hpp"

#include <curl/curl.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <utility>

#include "common/url.hpp"
#include "openalex/parser.hpp"

namespace schools::openalex {
namespace {

constexpr std::int64_t kConnectTimeoutSeconds = 20;
constexpr std::int64_t kRequestTimeoutSeconds = 60;
constexpr std::size_t kMaxErrorBody = 2048;

Status EnsureCurlInitialized() {
  static const Status kStatus = []() -> Status {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      return MakeError(Errc::kNetwork, "curl_global_init failed");
    }
    return {};
  }();
  return kStatus;
}

struct CurlHolder {
  CurlHolder() = default;
  CURL* handle{curl_easy_init()};

  ~CurlHolder() {
    if (handle != nullptr) {
      curl_easy_cleanup(handle);
    }
  }

  CurlHolder(const CurlHolder&) = delete;
  CurlHolder& operator=(const CurlHolder&) = delete;
  CurlHolder(CurlHolder&&) = delete;
  CurlHolder& operator=(CurlHolder&&) = delete;
};

Result<CURL*> ThreadCurl() {
  thread_local const CurlHolder kHolder;
  if (kHolder.handle == nullptr) {
    return MakeError(Errc::kNetwork, "curl_easy_init failed");
  }
  curl_easy_reset(kHolder.handle);
  return kHolder.handle;
}

std::size_t WriteCallback(void* contents, std::size_t size, std::size_t count,
                          void* user_data) {
  const std::size_t total = size * count;
  auto* body = static_cast<std::string*>(user_data);
  body->append(static_cast<const char*>(contents), total);
  return total;
}

std::string ToLower(std::string_view text) {
  std::string lower;
  lower.reserve(text.size());
  for (const char character : text) {
    lower.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }
  return lower;
}

std::size_t HeaderCallback(char* buffer, std::size_t size, std::size_t count,
                           void* user_data) {
  const std::size_t total = size * count;
  const std::string_view line(buffer, total);
  auto* response = static_cast<OpenAlexClient::Response*>(user_data);
  constexpr std::string_view kRetryAfter = "retry-after:";
  if (ToLower(line.substr(0, std::min(line.size(), kRetryAfter.size()))) ==
      kRetryAfter) {
    const std::string value(line.substr(kRetryAfter.size()));
    const auto first = value.find_first_not_of(" \t\r\n");
    const auto last = value.find_last_not_of(" \t\r\n");
    if (first != std::string::npos && last != std::string::npos) {
      response->retry_after = value.substr(first, last - first + 1);
    }
  }
  return total;
}

std::string Trim(std::string_view text, std::size_t limit) {
  return std::string(text.substr(0, std::min(text.size(), limit)));
}

std::string ExtractNumericId(std::string_view openalex_id) {
  const auto slash = openalex_id.find_last_of('/');
  return std::string{slash == std::string_view::npos
                         ? openalex_id
                         : openalex_id.substr(slash + 1)};
}

std::string BuildQuerySuffix(const ClientOptions& options) {
  std::string suffix;
  if (!options.mailto.empty()) {
    suffix += "&mailto=" + UrlEncode(options.mailto);
  }
  if (!options.api_key.empty()) {
    suffix += "&api_key=" + UrlEncode(options.api_key);
  }
  return suffix;
}

std::chrono::milliseconds BackoffDelay(int attempt) {
  const int capped = std::min(attempt, 6);
  return std::chrono::milliseconds{std::int64_t{500} << capped};
}

}  // namespace

OpenAlexClient::OpenAlexClient(ClientOptions options, RateLimiter& limiter)
    : options_{std::move(options)}, limiter_{limiter} {}

Result<std::vector<domain::Author>> OpenAlexClient::FetchAuthorsPage(
    int page, int per_page) {
  const std::string url =
      options_.base_url +
      "/authors?filter=last_known_institutions.country_code:RU"
      "&per_page=" +
      std::to_string(per_page) + "&page=" + std::to_string(page) +
      BuildQuerySuffix(options_);
  auto body = Get(url);
  if (!body) {
    return std::unexpected(body.error());
  }
  return ParseAuthors(*body);
}

Result<std::vector<domain::Work>> OpenAlexClient::FetchWorksPage(
    std::span<const std::string> author_ids,
    const std::unordered_set<std::string>& selected_authors, int page,
    int per_page) {
  std::string filter;
  for (const std::string& author_id : author_ids) {
    const std::string numeric_id = ExtractNumericId(author_id);
    if (numeric_id.empty()) {
      continue;
    }
    if (!filter.empty()) {
      filter.push_back('|');
    }
    filter += numeric_id;
  }

  const std::string url =
      options_.base_url +
      "/works?filter=authorships.author.id:" + UrlEncode(filter) +
      "&select=id,title,publication_year,authorships&per_page=" +
      std::to_string(per_page) + "&page=" + std::to_string(page) +
      BuildQuerySuffix(options_);
  auto body = Get(url);
  if (!body) {
    return std::unexpected(body.error());
  }
  return ParseWorks(*body, selected_authors);
}

Result<OpenAlexClient::Response> OpenAlexClient::Perform(
    std::string_view url) const {
  const auto initialized = EnsureCurlInitialized();
  if (!initialized) {
    return std::unexpected(initialized.error());
  }

  auto curl_result = ThreadCurl();
  if (!curl_result) {
    return std::unexpected(curl_result.error());
  }
  CURL* curl = *curl_result;

  const std::string url_string{url};
  Response response{};
  curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, std::int64_t{1});
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, kRequestTimeoutSeconds);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, std::int64_t{1});
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, std::int64_t{5});
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, std::int64_t{1});

  const std::string user_agent =
      options_.mailto.empty()
          ? "science-schools-backend/0.1"
          : "science-schools-backend/0.1 (mailto:" + options_.mailto + ")";
  curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());

  curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Accept: application/json");
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
  curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);

  const CURLcode result = curl_easy_perform(curl);
  if (headers != nullptr) {
    curl_slist_free_all(headers);
  }

  if (result != CURLE_OK) {
    return MakeError(Errc::kNetwork, "OpenAlex request failed: {}",
                     curl_easy_strerror(result));
  }

  std::int64_t status{0};
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  response.status = status;
  return response;
}

Result<std::string> OpenAlexClient::Get(std::string_view url) {
  for (int attempt = 0; attempt <= options_.max_retries; ++attempt) {
    limiter_.Acquire();

    auto response = Perform(url);
    if (!response) {
      if (attempt == options_.max_retries) {
        return std::unexpected(response.error());
      }
      const std::chrono::milliseconds delay = BackoffDelay(attempt);
      spdlog::warn("OpenAlex request failed: {}, retry {} in {} ms",
                   response.error().message, attempt + 1, delay.count());
      std::this_thread::sleep_for(delay);
      continue;
    }
    if (response->status == 200) {
      return std::move(response->body);
    }

    const bool retryable = response->status == 429 || response->status == 0 ||
                           response->status >= 500;
    if (!retryable || attempt == options_.max_retries) {
      return MakeError(Errc::kNetwork, "OpenAlex HTTP {}: {}", response->status,
                       Trim(response->body, kMaxErrorBody));
    }

    std::chrono::milliseconds delay = BackoffDelay(attempt);
    if (!response->retry_after.empty()) {
      std::int64_t seconds{0};
      const char* begin = response->retry_after.data();
      const char* end = begin + response->retry_after.size();
      const auto [ptr, error] = std::from_chars(begin, end, seconds);
      if (error == std::errc{} && ptr == end && seconds > 0 && seconds < 300) {
        delay = std::chrono::seconds(seconds);
      }
    }

    spdlog::warn("OpenAlex request failed with HTTP {}, retry {} in {} ms",
                 response->status, attempt + 1, delay.count());
    std::this_thread::sleep_for(delay);
  }
  return MakeError(Errc::kNetwork, "OpenAlex retries exhausted");
}

}  // namespace schools::openalex

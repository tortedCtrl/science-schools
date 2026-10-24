#include "common/config.hpp"

#include <cstdlib>
#include <string>

#include "common/parse.hpp"

namespace schools {
namespace {

template <typename T>
Status LoadNumber(const char* name, T& target) {
  const char* value{std::getenv(name)};
  if (value == nullptr || *value == '\0') {
    return {};
  }
  const auto parsed{ParseNumber<T>(value, name)};
  if (!parsed) {
    return std::unexpected(parsed.error());
  }
  target = *parsed;
  return {};
}

void LoadString(const char* name, std::string& target) {
  const char* value{std::getenv(name)};
  if (value != nullptr && *value != '\0') {
    target = value;
  }
}

}  // namespace

Result<Config> Config::FromEnv() {
  Config c;
  Status status;
  // Keeps the first error and skips the remaining numeric settings.
  const auto number{[&status](const char* name, auto& target) {
    if (status) {
      status = LoadNumber(name, target);
    }
  }};

  LoadString("DB_HOST", c.db_host);
  number("DB_PORT", c.db_port);
  LoadString("DB_USER", c.db_user);
  LoadString("DB_PASSWORD", c.db_password);
  LoadString("DB_NAME", c.db_name);
  number("DB_POOL_SIZE", c.db_pool_size);

  LoadString("LOG_LEVEL", c.log_level);
  number("IO_THREADS", c.io_threads);

  LoadString("OPENALEX_HOST", c.openalex_host);
  LoadString("OPENALEX_MAILTO", c.openalex_mailto);
  LoadString("OPENALEX_API_KEY", c.openalex_api_key);
  LoadString("COUNTRY_CODE", c.country_code);
  number("OPENALEX_PAGE_SIZE", c.openalex_page_size);
  number("OPENALEX_RPS", c.openalex_rps);
  number("INGEST_CONCURRENCY", c.ingest_concurrency);

  number("MAX_RU_AUTHORS_PER_WORK", c.max_ru_authors_per_work);
  number("LEIDEN_RESOLUTION", c.leiden_resolution);
  number("LEIDEN_THREADS", c.leiden_threads);

  LoadString("HTTP_ADDRESS", c.http_address);
  number("HTTP_PORT", c.http_port);

  if (!status) {
    return std::unexpected(status.error());
  }
  return c;
}

}  // namespace schools

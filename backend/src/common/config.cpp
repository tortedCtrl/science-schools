#include "common/config.hpp"

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "common/parse.hpp"

namespace schools {
namespace {

std::string EnvString(const char* name, std::string fallback = {}) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return fallback;
  }
  return value;
}

Result<std::string> RequiredEnv(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return MakeError(Errc::kInvalidArgument,
                     "required environment variable is not set: {}", name);
  }
  return std::string(value);
}

Result<int> EnvInt(const char* name, int fallback, int min_value,
                   int max_value) {
  const std::string text = EnvString(name);
  if (text.empty()) {
    return fallback;
  }
  const auto value = ParseNumber<int>(text, name);
  if (!value) {
    return std::unexpected(value.error());
  }
  if (*value < min_value || *value > max_value) {
    return MakeError(Errc::kInvalidArgument,
                     "value for {} must be between {} and {}", name, min_value,
                     max_value);
  }
  return *value;
}

Result<double> EnvDouble(const char* name, double fallback, double min_value,
                         double max_value) {
  const std::string text = EnvString(name);
  if (text.empty()) {
    return fallback;
  }
  const auto value = ParseNumber<double>(text, name);
  if (!value) {
    return std::unexpected(value.error());
  }
  if (!std::isfinite(*value) || *value < min_value || *value > max_value) {
    return MakeError(Errc::kInvalidArgument,
                     "value for {} must be between {} and {}", name, min_value,
                     max_value);
  }
  return *value;
}

Result<bool> EnvBool(const char* name, bool fallback) {
  const std::string text = EnvString(name);
  if (text.empty()) {
    return fallback;
  }
  if (text == "1" || text == "true" || text == "TRUE" || text == "yes") {
    return true;
  }
  if (text == "0" || text == "false" || text == "FALSE" || text == "no") {
    return false;
  }
  return MakeError(Errc::kInvalidArgument, "invalid boolean value for {}: {}",
                   name, text);
}

template <typename T>
void Apply(std::optional<Error>& error, T& target, Result<T> value) {
  if (error.has_value()) {
    return;
  }
  if (value.has_value()) {
    target = std::move(*value);
    return;
  }
  error = std::move(value.error());
}

}  // namespace

Result<Config> Config::FromEnv() {
  auto connection = RequiredEnv("ConnectionStrings__DefaultConnection");
  if (!connection) {
    return std::unexpected(connection.error());
  }

  Config config{};
  config.db_connection = std::move(*connection);

  std::optional<Error> error;
  Apply(error, config.db_pool_size,
        EnvInt("DB_POOL_SIZE", config.db_pool_size, 1, 128));

  config.log_level = EnvString("LOG_LEVEL", config.log_level);
  config.backend_host = EnvString("BACKEND_HOST", config.backend_host);
  Apply(error, config.backend_port,
        EnvInt("BACKEND_PORT", config.backend_port, 1, 65535));
  config.frontend_port = EnvString("FRONTEND_PORT", config.frontend_port);

  Apply(error, config.ingest_max_authors,
        EnvInt("INGEST_MAX_AUTHORS", config.ingest_max_authors, 1, 100000));
  Apply(error, config.ingest_author_page_size,
        EnvInt("INGEST_AUTHOR_PAGE_SIZE", config.ingest_author_page_size, 1,
               200));
  Apply(error, config.ingest_work_page_size,
        EnvInt("INGEST_WORK_PAGE_SIZE", config.ingest_work_page_size, 1, 200));
  Apply(error, config.ingest_work_pages,
        EnvInt("INGEST_WORK_PAGES", config.ingest_work_pages, 1, 1000));
  Apply(error, config.ingest_work_batch_size,
        EnvInt("INGEST_WORK_BATCH_SIZE", config.ingest_work_batch_size, 1, 50));
  Apply(error, config.ingest_concurrency,
        EnvInt("INGEST_CONCURRENCY", config.ingest_concurrency, 1, 64));
  Apply(error, config.ingest_on_start,
        EnvBool("INGEST_ON_START", config.ingest_on_start));

  Apply(error, config.openalex_rps,
        EnvDouble("OPENALEX_RPS", config.openalex_rps, 0.1, 100.0));
  Apply(error, config.openalex_max_retries,
        EnvInt("OPENALEX_MAX_RETRIES", config.openalex_max_retries, 0, 20));
  config.openalex_mailto = EnvString("OPENALEX_MAILTO");
  config.openalex_api_key = EnvString("OPENALEX_API_KEY");

  if (error.has_value()) {
    return std::unexpected(std::move(*error));
  }
  return config;
}

}  // namespace schools

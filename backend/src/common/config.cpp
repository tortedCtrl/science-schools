#include "common/config.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

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

template <typename T>
T ReadValue(const nlohmann::json& json, const char* name) {
  return json.at(name).get<T>();
}

void ValidateRange(int value, const char* name, int min_value, int max_value) {
  if (value < min_value || value > max_value) {
    throw std::invalid_argument(
        fmt::format("value for {} must be between {} and {}", name, min_value,
                    max_value));
  }
}

void ValidateLogLevel(const std::string& value) {
  if (value != "trace" && value != "debug" && value != "info" &&
      value != "warning" && value != "warn" && value != "error" &&
      value != "err" && value != "critical" && value != "off") {
    throw std::invalid_argument(
        fmt::format("unsupported log_level: {}", value));
  }
}

void OverrideStringFromEnv(std::string& value, const char* name) {
  const char* env_value = std::getenv(name);
  if (env_value != nullptr && *env_value != '\0') {
    value = env_value;
  }
}

void OverrideIntFromEnv(int& value, const char* name, int min_value,
                        int max_value) {
  const char* env_value = std::getenv(name);
  if (env_value == nullptr || *env_value == '\0') {
    return;
  }

  int parsed{};
  const std::string_view text{env_value};
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (error != std::errc{} || end != text.data() + text.size()) {
    throw std::invalid_argument(
        fmt::format("invalid value for {}: {}", name, text));
  }
  value = parsed;
  ValidateRange(value, name, min_value, max_value);
}

void ValidateDoubleRange(double value, const char* name, double min_value,
                         double max_value) {
  if (!std::isfinite(value) || value < min_value || value > max_value) {
    throw std::invalid_argument(
        fmt::format("value for {} must be between {} and {}", name, min_value,
                    max_value));
  }
}

}  // namespace

Result<Config> Config::Load() {
  auto connection = RequiredEnv("ConnectionStrings__DefaultConnection");
  if (!connection) {
    return std::unexpected(connection.error());
  }

  const std::string config_path = EnvString("CONFIG_PATH", "config.json");
  std::ifstream file(config_path);
  if (!file) {
    return MakeError(Errc::kInvalidArgument,
                     "cannot open configuration file: {}", config_path);
  }

  try {
    const nlohmann::json json = nlohmann::json::parse(file);
    Config config{};
    config.db_connection = std::move(*connection);

    config.db_pool_size = ReadValue<int>(json, "db_pool_size");
    config.log_level = ReadValue<std::string>(json, "log_level");
    config.show_timer_logs = ReadValue<bool>(json, "show_timer_logs");
    config.backend_host = ReadValue<std::string>(json, "backend_host");
    config.backend_port = ReadValue<int>(json, "backend_port");
    config.frontend_port = ReadValue<std::string>(json, "frontend_port");

    config.ingest_max_authors = ReadValue<int>(json, "ingest_max_authors");
    config.ingest_author_page_size =
        ReadValue<int>(json, "ingest_author_page_size");
    config.ingest_work_page_size =
        ReadValue<int>(json, "ingest_work_page_size");
    config.ingest_work_pages = ReadValue<int>(json, "ingest_work_pages");
    config.ingest_work_batch_size =
        ReadValue<int>(json, "ingest_work_batch_size");
    config.ingest_concurrency = ReadValue<int>(json, "ingest_concurrency");
    config.download_authors = ReadValue<bool>(json, "download_authors");
    config.bigclam_binary = ReadValue<std::string>(json, "bigclam_binary");
    config.bigclam_communities =
        ReadValue<int>(json, "bigclam_communities");
    config.bigclam_threads = ReadValue<int>(json, "bigclam_threads");

    config.openalex_rps = ReadValue<double>(json, "openalex_rps");
    config.openalex_max_retries = ReadValue<int>(json, "openalex_max_retries");
    config.openalex_mailto = EnvString("OPENALEX_MAILTO");
    config.openalex_api_key = EnvString("OPENALEX_API_KEY");

    OverrideStringFromEnv(config.log_level, "LOG_LEVEL");
    OverrideStringFromEnv(config.backend_host, "BACKEND_HOST");
    OverrideStringFromEnv(config.frontend_port, "FRONTEND_PORT");
    OverrideIntFromEnv(config.db_pool_size, "DB_POOL_SIZE", 1, 128);
    OverrideIntFromEnv(config.backend_port, "BACKEND_PORT", 1, 65535);

    ValidateRange(config.db_pool_size, "db_pool_size", 1, 128);
    ValidateRange(config.backend_port, "backend_port", 1, 65535);
    ValidateLogLevel(config.log_level);
    ValidateRange(config.ingest_max_authors, "ingest_max_authors", 1, 100000);
    ValidateRange(config.ingest_author_page_size, "ingest_author_page_size", 1,
                  200);
    ValidateRange(config.ingest_work_page_size, "ingest_work_page_size", 1,
                  200);
    ValidateRange(config.ingest_work_pages, "ingest_work_pages", 1, 1000);
    ValidateRange(config.ingest_work_batch_size, "ingest_work_batch_size", 1,
                  50);
    ValidateRange(config.ingest_concurrency, "ingest_concurrency", 1, 64);
    ValidateRange(config.bigclam_communities, "bigclam_communities", 1, 100000);
    ValidateRange(config.bigclam_threads, "bigclam_threads", 1, 256);
    ValidateRange(config.openalex_max_retries, "openalex_max_retries", 0, 20);
    ValidateDoubleRange(config.openalex_rps, "openalex_rps", 0.1, 100.0);
    return config;
  } catch (const nlohmann::json::exception& error) {
    return MakeError(Errc::kInvalidArgument,
                     "invalid configuration file {}: {}", config_path,
                     error.what());
  } catch (const std::invalid_argument& error) {
    return MakeError(Errc::kInvalidArgument,
                     "invalid configuration value: {}", error.what());
  }
}

}  // namespace schools

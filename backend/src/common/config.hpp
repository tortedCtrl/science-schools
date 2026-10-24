#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "common/error.hpp"

namespace schools {

// All settings come from environment variables (see Config::FromEnv).
struct Config {
  // MySQL / MariaDB.
  std::string db_host{"localhost"};
  std::uint16_t db_port{3306};
  std::string db_user{"schools"};
  std::string db_password{"schools"};
  std::string db_name{"schools"};
  std::size_t db_pool_size{8};  // max connections in the pool

  std::string log_level{"info"};
  int io_threads{4};  // threads running the single io_context

  // OpenAlex ingestion.
  std::string openalex_host{"api.openalex.org"};
  std::string openalex_mailto;   // enables the "polite pool"
  std::string openalex_api_key;  // optional
  std::string country_code{"RU"};
  int openalex_page_size{200};
  double openalex_rps{8.0};   // global request rate limit
  int ingest_concurrency{8};  // partitions processed in parallel

  // Graph construction / clustering.
  int max_ru_authors_per_work{50};  // skip mega-papers (clique explosion)
  double leiden_resolution{1.0};
  int leiden_threads{0};  // 0 = OpenMP default

  // HTTP API.
  std::string http_address{"0.0.0.0"};
  std::uint16_t http_port{8080};

  static Result<Config> FromEnv();
};

}  // namespace schools

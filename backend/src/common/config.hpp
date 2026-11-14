#pragma once

#include <string>

#include "common/error.hpp"

namespace schools {

// Runtime configuration. Everything is read from environment variables once,
// at process start, so the rest of the code never touches getenv().
struct Config {
  std::string db_connection;
  int db_pool_size{8};

  std::string log_level{"info"};

  std::string backend_host{"0.0.0.0"};
  int backend_port{8080};
  std::string frontend_port{"5173"};

  // OpenAlex ingestion. The defaults match the pre-refactor server: fetch a
  // 500-author sample, while INGEST_CONCURRENCY makes the requests parallel.
  int ingest_max_authors{500};
  int ingest_author_page_size{10};
  int ingest_work_page_size{20};
  int ingest_work_pages{2};
  int ingest_work_batch_size{10};
  int ingest_concurrency{4};
  bool ingest_on_start{true};

  double openalex_rps{8.0};
  int openalex_max_retries{5};
  std::string openalex_mailto;
  std::string openalex_api_key;

  static Result<Config> FromEnv();
};

}  // namespace schools

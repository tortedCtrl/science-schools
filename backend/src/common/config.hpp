#pragma once

#include <string>

#include "common/error.hpp"

namespace schools {

// Runtime settings come from config.json; credentials and deployment-specific
// network overrides remain in environment variables.
struct Config {
  std::string db_connection;
  int db_pool_size;

  // spdlog threshold: trace, debug, info, warning, error, critical, or off.
  std::string log_level;
  bool show_timer_logs;

  std::string backend_host;
  int backend_port;
  std::string frontend_port;

  int ingest_max_authors;
  int ingest_author_page_size;
  int ingest_work_page_size;
  int ingest_work_pages;
  int ingest_work_batch_size;
  int ingest_concurrency;
  bool download_authors;
  std::string bigclam_binary;
  int bigclam_communities;
  int bigclam_threads;

  double openalex_rps;
  int openalex_max_retries;
  std::string openalex_mailto;
  std::string openalex_api_key;

  static Result<Config> Load();
};

}  // namespace schools

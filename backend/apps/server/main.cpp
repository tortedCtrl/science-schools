#include <httplib.h>
#include <spdlog/spdlog.h>

#include <string>

#include "api/routes.hpp"
#include "common/config.hpp"
#include "repository/author_repository.hpp"
#include "repository/community_repository.hpp"
#include "repository/database.hpp"
#include "repository/graph_repository.hpp"
#include "repository/ingest_repository.hpp"
#include "repository/schema.hpp"
#include "service/ingest_service.hpp"
#include "service/query_service.hpp"

namespace {

void SetCorsHeaders(httplib::Response& response) {
  response.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
  response.set_header("Access-Control-Allow-Headers", "Content-Type, Accept");
}

}  // namespace

int main() {
  auto config = schools::Config::FromEnv();
  if (!config) {
    spdlog::error("Invalid configuration: {}", config.error().message);
    return 2;
  }

  spdlog::set_level(spdlog::level::from_str(config->log_level));
  spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [t%t] %v");

  spdlog::info("Connecting to PostgreSQL");
  schools::repository::Database database;
  auto opened = database.Open(config->db_connection, config->db_pool_size);
  if (!opened) {
    spdlog::error("{}", opened.error().message);
    return 1;
  }

  auto schema = schools::repository::EnsureSchema(database);
  if (!schema) {
    spdlog::error("{}", schema.error().message);
    return 1;
  }

  schools::repository::AuthorRepository author_repository(database);
  schools::repository::GraphRepository graph_repository(database);
  schools::service::QueryService query_service(author_repository,
                                               graph_repository);

  if (config->ingest_on_start) {
    schools::repository::IngestRepository ingest_repository(database);
    schools::repository::CommunityRepository community_repository(database);
    schools::service::IngestService ingest_service(*config, ingest_repository,
                                                   community_repository);
    auto ingested = ingest_service.Run();
    if (!ingested) {
      spdlog::error("{}", ingested.error().message);
      return 1;
    }
  } else {
    spdlog::info("INGEST_ON_START=false: using existing database contents");
  }

  httplib::Server server;
  schools::api::RegisterRoutes(server, query_service);

  const std::string frontend_port = config->frontend_port;
  server.set_post_routing_handler(
      [frontend_port](const httplib::Request& request,
                      httplib::Response& response) {
        const std::string origin = request.get_header_value("Origin");
        if (origin == "http://127.0.0.1:" + frontend_port ||
            origin == "http://localhost:" + frontend_port) {
          response.set_header("Access-Control-Allow-Origin", origin);
        }
        SetCorsHeaders(response);
      });

  spdlog::info("Listening on {}:{}", config->backend_host,
               config->backend_port);
  if (!server.listen(config->backend_host, config->backend_port)) {
    spdlog::error("Failed to start HTTP server");
    return 1;
  }

  return 0;
}

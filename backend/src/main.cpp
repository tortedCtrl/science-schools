#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "api/controller.hpp"
#include "api/http_server.hpp"
#include "api/router.hpp"
#include "common/config.hpp"
#include "common/error.hpp"
#include "common/parse.hpp"
#include "graph/leiden.hpp"
#include "repository/author_repository.hpp"
#include "repository/community_repository.hpp"
#include "repository/database.hpp"
#include "repository/graph_repository.hpp"
#include "repository/ingest_repository.hpp"
#include "repository/schema.hpp"
#include "service/clustering_service.hpp"
#include "service/ingest_service.hpp"
#include "service/query_service.hpp"

namespace {

using schools::Config;
using schools::Error;
using schools::Result;
using schools::Status;
namespace asio = boost::asio;

constexpr std::string_view kUsage{
    "Usage: scientific_schools <command> [options]\n"
    "  serve                       run the HTTP API\n"
    "  migrate                     create/upgrade the database schema\n"
    "  ingest [--from-year=N] [--to-year=N] [--force]\n"
    "                              download works from OpenAlex into the DB\n"
    "  cluster                     rebuild the graph and run Leiden\n"
    "Settings come from environment variables (see README).\n"};

struct IngestArgs {
  schools::service::IngestOptions options;
};

Result<IngestArgs> ParseIngestArgs(const std::vector<std::string>& args) {
  IngestArgs result;
  for (const std::string& arg : args) {
    if (arg == "--force") {
      result.options.force = true;
    } else if (arg.starts_with("--from-year=")) {
      auto year{schools::ParseNumber<int>(arg.substr(12), "--from-year")};
      if (!year) {
        return std::unexpected(year.error());
      }
      result.options.from_year = *year;
    } else if (arg.starts_with("--to-year=")) {
      auto year{schools::ParseNumber<int>(arg.substr(10), "--to-year")};
      if (!year) {
        return std::unexpected(year.error());
      }
      result.options.to_year = *year;
    } else {
      return schools::MakeError(schools::Errc::kInvalidArgument,
                                "Unknown option '{}'", arg);
    }
  }
  if (result.options.from_year > result.options.to_year) {
    return schools::MakeError(schools::Errc::kInvalidArgument,
                              "--from-year must not exceed --to-year");
  }
  return result;
}

// Everything the commands need, wired together (composition root).
struct App {
  App(asio::io_context& ioc, const Config& config)
      : db{ioc, config},
        authors{db},
        communities{db},
        graph{db},
        ingest_repo{db},
        // Leiden is CPU-bound: give it its own thread so I/O threads stay free.
        compute{1},
        detector{schools::graph::MakeLeidenDetector()} {}

  schools::repository::Database db;
  schools::repository::AuthorRepository authors;
  schools::repository::CommunityRepository communities;
  schools::repository::GraphRepository graph;
  schools::repository::IngestRepository ingest_repo;
  asio::thread_pool compute;
  std::unique_ptr<schools::graph::CommunityDetector> detector;
};

asio::awaitable<Status> RunMigrate(App* app) {
  co_return co_await schools::repository::Migrate(&app->db);
}

asio::awaitable<Status> RunIngest(App* app, const Config* config,
                                  schools::service::IngestOptions options) {
  if (auto status = co_await schools::repository::Migrate(&app->db); !status) {
    co_return status;
  }
  schools::service::IngestService service(*config, app->ingest_repo);
  co_return co_await service.Run(options);
}

asio::awaitable<Status> RunCluster(App* app, const Config* config) {
  if (auto status = co_await schools::repository::Migrate(&app->db); !status) {
    co_return status;
  }
  schools::service::ClusteringService service(
      app->graph, app->communities, *app->detector, app->compute.get_executor(),
      {.max_ru_authors_per_work = config->max_ru_authors_per_work,
       .resolution = config->leiden_resolution,
       .threads = config->leiden_threads});
  auto summary = co_await service.Run();
  if (!summary) {
    co_return std::unexpected(summary.error());
  }
  spdlog::info(
      "Clustering run {} active: {} authors, {} edges, {} communities, "
      "modularity {:.4f}",
      summary->run_id, summary->vertices, summary->edges, summary->communities,
      summary->modularity);
  co_return Status{};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    spdlog::error("{}", kUsage);
    return 2;
  }
  const std::string command{argv[1]};
  const std::vector<std::string> args(argv + 2, argv + argc);

  auto config{Config::FromEnv()};
  if (!config) {
    spdlog::error("Invalid configuration: {}", config.error().message);
    return 2;
  }
  spdlog::set_level(spdlog::level::from_str(config->log_level));
  spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [t%t] %v");

  asio::io_context ioc;
  App app(ioc, *config);
  app.db.Start();

  // Commands run as one coroutine; its Status becomes the exit code.
  int exit_code{0};
  const auto on_done{
      [&](const std::exception_ptr& exception, const Status& status) {
        if (exception) {
          spdlog::error("Command terminated unexpectedly");
          exit_code = 1;
        } else if (!status) {
          spdlog::error("{}", status.error().message);
          exit_code = 1;
        }
        app.db.Cancel();
        ioc.stop();
      }};

  // Declared at this scope so they outlive ioc.run().
  schools::api::Router router;
  std::unique_ptr<schools::service::QueryService> query_service;
  std::unique_ptr<schools::api::ApiController> controller;
  std::unique_ptr<schools::api::HttpServer> server;

  if (command == "migrate") {
    asio::co_spawn(ioc, RunMigrate(&app), on_done);
  } else if (command == "ingest") {
    auto parsed{ParseIngestArgs(args)};
    if (!parsed) {
      spdlog::error("{}\n{}", parsed.error().message, kUsage);
      return 2;
    }
    asio::co_spawn(ioc, RunIngest(&app, &*config, parsed->options), on_done);
  } else if (command == "cluster") {
    asio::co_spawn(ioc, RunCluster(&app, &*config), on_done);
  } else if (command == "serve") {
    query_service = std::make_unique<schools::service::QueryService>(
        app.authors, app.communities, app.graph);
    controller = std::make_unique<schools::api::ApiController>(*query_service);
    controller->Register(router);
    server = std::make_unique<schools::api::HttpServer>(
        ioc,
        schools::api::ServerOptions{config->http_address, config->http_port},
        router);
    // Schema must exist before the first request.
    asio::co_spawn(
        ioc,
        [&]() -> asio::awaitable<Status> {
          if (auto status = co_await schools::repository::Migrate(&app.db);
              !status) {
            co_return status;
          }
          co_return server->Start();
        },
        [&](const std::exception_ptr& exception, const Status& status) {
          if (exception || !status) {
            on_done(exception, status);
          }
        });
  } else {
    spdlog::error("Unknown command '{}'\n{}", command, kUsage);
    return 2;
  }

  // Ctrl+C / SIGTERM: stop cleanly.
  asio::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait([&](const boost::system::error_code& ec, int) {
    if (ec) {
      return;
    }
    spdlog::info("Signal received, shutting down");
    app.db.Cancel();
    ioc.stop();
  });

  std::vector<std::jthread> threads;
  for (int i = 1; i < config->io_threads; ++i) {
    threads.emplace_back([&ioc] { ioc.run(); });
  }
  ioc.run();
  threads.clear();  // join
  app.compute.join();
  return exit_code;
}

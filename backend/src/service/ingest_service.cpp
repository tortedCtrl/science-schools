#include "service/ingest_service.hpp"

#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/ssl.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/rate_limiter.hpp"
#include "openalex/client.hpp"

namespace schools::service {
namespace asio = boost::asio;

namespace {

struct Partition {
  std::string key;  // e.g. "RU:2023-05"
  std::string from_date;
  std::string to_date;
};

std::vector<Partition> BuildPartitions(int from_year, int to_year,
                                       const std::string& country) {
  std::vector<Partition> partitions;
  for (int year = from_year; year <= to_year; ++year) {
    for (unsigned month = 1; month <= 12; ++month) {
      const std::chrono::year_month_day_last last{
          std::chrono::year{year},
          std::chrono::month_day_last{std::chrono::month{month}}};
      partitions.push_back({fmt::format("{}:{:04}-{:02}", country, year, month),
                            fmt::format("{:04}-{:02}-01", year, month),
                            fmt::format("{:04}-{:02}-{:02}", year, month,
                                        static_cast<unsigned>(last.day()))});
    }
  }
  return partitions;
}

// State shared by all workers of one Run().
struct RunContext {
  RunContext(const Config& cfg, repository::IngestRepository& repo,
             const IngestOptions& opts, std::vector<Partition> parts)
      : config{cfg},
        repository{repo},
        options{opts},
        partitions{std::move(parts)},
        ssl_context{asio::ssl::context::tls_client},
        limiter{std::make_shared<RateLimiter>(cfg.openalex_rps)} {
    boost::system::error_code ec;
    ssl_context.set_default_verify_paths(ec);
    if (ec) {
      spdlog::warn("Could not load default TLS trust store: {}", ec.message());
    }
    ssl_context.set_verify_mode(asio::ssl::verify_peer, ec);
  }

  const Config& config;
  repository::IngestRepository& repository;
  IngestOptions options;
  std::vector<Partition> partitions;
  std::atomic<std::size_t> next_partition{0};
  std::atomic<std::int64_t> works_total{0};
  std::atomic<std::size_t> failed_partitions{0};
  asio::ssl::context ssl_context;
  std::shared_ptr<RateLimiter> limiter;
};

asio::awaitable<Status> ProcessPartition(RunContext* ctx,
                                         openalex::OpenAlexClient* client,
                                         Partition partition) {
  repository::PartitionState state;
  if (ctx->options.force) {
    auto reset = co_await ctx->repository.ResetPartition(partition.key);
    if (!reset) {
      co_return reset;
    }
  } else {
    auto saved = co_await ctx->repository.LoadState(partition.key);
    if (!saved) {
      co_return std::unexpected(saved.error());
    }
    if (*saved) {
      state = **saved;
    }
  }
  if (state.done) {
    spdlog::debug("Partition {} already done", partition.key);
    co_return Status{};
  }

  const openalex::WorksQuery query{ctx->config.country_code,
                                   partition.from_date, partition.to_date,
                                   ctx->config.openalex_page_size};
  std::int64_t partition_works{0};
  std::string cursor{state.cursor};
  while (true) {
    auto page = co_await client->FetchWorksPage(query, cursor);
    if (!page) {
      co_return std::unexpected(page.error());
    }

    repository::PartitionState next;
    next.done = page->next_cursor.empty();
    next.cursor = next.done ? cursor : page->next_cursor;
    const auto page_works{static_cast<std::int64_t>(page->works.size())};

    auto saved = co_await ctx->repository.SavePage(partition.key,
                                                   std::move(*page), next);
    if (!saved) {
      co_return saved;
    }

    partition_works += page_works;
    ctx->works_total += page_works;
    spdlog::debug("{}: saved {} works (partition total {})", partition.key,
                  page_works, partition_works);
    if (next.done) {
      break;
    }
    cursor = next.cursor;
  }
  spdlog::info("Partition {} done: {} works", partition.key, partition_works);
  co_return Status{};
}

// Pulls partitions from the shared counter until none are left.
asio::awaitable<void> Worker(RunContext* ctx) {
  const auto executor = co_await asio::this_coro::executor;
  openalex::OpenAlexClient client(executor, ctx->ssl_context,
                                  {.host = ctx->config.openalex_host,
                                   .mailto = ctx->config.openalex_mailto,
                                   .api_key = ctx->config.openalex_api_key},
                                  ctx->limiter);

  while (true) {
    const std::size_t index{ctx->next_partition.fetch_add(1)};
    if (index >= ctx->partitions.size()) {
      co_return;
    }
    const Partition& partition{ctx->partitions[index]};

    auto status = co_await ProcessPartition(ctx, &client, partition);
    if (!status) {
      ++ctx->failed_partitions;
      spdlog::error("Partition {} failed: {} (will resume on next run)",
                    partition.key, status.error().message);
    }
  }
}

}  // namespace

asio::awaitable<Status> IngestService::Run(IngestOptions options) {
  auto partitions{BuildPartitions(options.from_year, options.to_year,
                                  config_.country_code)};
  spdlog::info("Ingest {}..{}: {} partitions, concurrency {}, {:.1f} req/s",
               options.from_year, options.to_year, partitions.size(),
               config_.ingest_concurrency, config_.openalex_rps);

  RunContext ctx(config_, repository_, options, std::move(partitions));
  const auto executor = co_await asio::this_coro::executor;

  using WorkerOp =
      decltype(asio::co_spawn(executor, Worker(&ctx), asio::deferred));
  std::vector<WorkerOp> workers;
  for (int i = 0; i < config_.ingest_concurrency; ++i) {
    workers.push_back(asio::co_spawn(executor, Worker(&ctx), asio::deferred));
  }
  // Waits for all workers; they run in parallel on the io_context threads.
  const auto [order, exceptions] =
      co_await asio::experimental::make_parallel_group(std::move(workers))
          .async_wait(asio::experimental::wait_for_all(), asio::use_awaitable);
  (void)order;
  for (const std::exception_ptr& exception : exceptions) {
    if (exception) {
      ++ctx.failed_partitions;
      spdlog::error("An ingest worker terminated unexpectedly");
    }
  }

  spdlog::info("Ingest finished: {} works saved, {} partition(s) failed",
               ctx.works_total.load(), ctx.failed_partitions.load());
  if (ctx.failed_partitions.load() > 0) {
    co_return MakeError(Errc::kInternal, "{} partition(s) failed",
                        ctx.failed_partitions.load());
  }
  co_return Status{};
}

}  // namespace schools::service

#include "service/clustering_service.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <vector>

#include "common/async.hpp"

namespace schools::service {
namespace {

using Clock = std::chrono::steady_clock;

double SecondsSince(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

// Leiden returns arbitrary community labels. Renumber them densely, largest
// community first, so community_id 0 is always the biggest school.
std::vector<std::int32_t> RenumberBySize(
    const std::vector<std::uint32_t>& membership,
    std::int32_t& community_count) {
  std::uint32_t max_label{0};
  for (const std::uint32_t label : membership) {
    max_label = std::max(max_label, label);
  }

  std::vector<std::uint32_t> sizes(static_cast<std::size_t>(max_label) + 1, 0);
  for (const std::uint32_t label : membership) {
    ++sizes[label];
  }

  std::vector<std::uint32_t> labels;
  for (std::uint32_t label = 0; label <= max_label; ++label) {
    if (sizes[label] > 0) {
      labels.push_back(label);
    }
  }
  std::ranges::stable_sort(labels, [&](std::uint32_t a, std::uint32_t b) {
    return sizes[a] > sizes[b];
  });

  std::vector<std::int32_t> new_id(sizes.size(), -1);
  for (std::size_t i = 0; i < labels.size(); ++i) {
    new_id[labels[i]] = static_cast<std::int32_t>(i);
  }
  community_count = static_cast<std::int32_t>(labels.size());

  std::vector<std::int32_t> result(membership.size());
  std::ranges::transform(membership, result.begin(),
                         [&](std::uint32_t label) { return new_id[label]; });
  return result;
}

}  // namespace

boost::asio::awaitable<Result<ClusteringSummary>> ClusteringService::Run() {
  const auto total_start{Clock::now()};

  auto step{Clock::now()};
  auto rebuilt = co_await graph_repo_.RebuildDerivedTables(
      options_.max_ru_authors_per_work);
  if (!rebuilt) {
    co_return std::unexpected(rebuilt.error());
  }
  spdlog::info("Rebuilt coauthor edges and author fields in {:.1f}s",
               SecondsSince(step));

  step = Clock::now();
  auto vertex_ids_result = co_await graph_repo_.LoadVertexIds();
  if (!vertex_ids_result) {
    co_return std::unexpected(vertex_ids_result.error());
  }
  const std::vector<std::int64_t> vertex_ids{std::move(*vertex_ids_result)};
  if (vertex_ids.empty()) {
    co_return MakeError(Errc::kUnavailable,
                        "No authors in the database; run ingest first");
  }
  const auto vertex_count{static_cast<std::uint32_t>(vertex_ids.size())};
  const auto index_of{[&vertex_ids](std::int64_t id) {
    return static_cast<std::uint32_t>(std::ranges::lower_bound(vertex_ids, id) -
                                      vertex_ids.begin());
  }};

  std::vector<graph::Edge> edges;
  std::vector<float> weighted_degree(vertex_count, 0.0F);
  auto streamed = co_await graph_repo_.ForEachEdge(
      [&](std::int64_t src, std::int64_t dst, float weight) {
        const std::uint32_t u{index_of(src)};
        const std::uint32_t v{index_of(dst)};
        edges.push_back({u, v, weight});
        weighted_degree[u] += weight;
        weighted_degree[v] += weight;
      });
  if (!streamed) {
    co_return std::unexpected(streamed.error());
  }
  spdlog::info("Loaded graph: {} vertices, {} edges in {:.1f}s", vertex_count,
               edges.size(), SecondsSince(step));

  step = Clock::now();
  const graph::DetectParams params{.resolution = options_.resolution,
                                   .threads = options_.threads};
  auto detected = co_await RunBlocking(compute_executor_, [&] {
    return detector_.Detect(vertex_count, edges, params);
  });
  if (!detected) {
    co_return std::unexpected(detected.error());
  }
  std::int32_t community_count{0};
  const std::vector<std::int32_t> community_of{
      RenumberBySize(detected->membership, community_count)};
  spdlog::info("Leiden: {} communities, modularity {:.4f}, {:.1f}s",
               community_count, detected->modularity, SecondsSince(step));

  step = Clock::now();
  auto run = co_await community_repo_.CreateRun(
      {.algorithm = "leiden",
       .resolution = options_.resolution,
       .vertices = static_cast<std::int64_t>(vertex_count),
       .edges = static_cast<std::int64_t>(edges.size())});
  if (!run) {
    co_return std::unexpected(run.error());
  }
  const std::int64_t run_id{*run};

  std::vector<domain::MembershipRow> rows(vertex_count);
  for (std::uint32_t i = 0; i < vertex_count; ++i) {
    rows[i] = {vertex_ids[i], community_of[i], weighted_degree[i]};
  }
  auto saved =
      co_await community_repo_.SaveMemberships(run_id, std::move(rows));
  if (!saved) {
    co_return std::unexpected(saved.error());
  }
  auto activated = co_await community_repo_.ActivateRun(
      run_id, detected->modularity, community_count);
  if (!activated) {
    co_return std::unexpected(activated.error());
  }
  spdlog::info("Saved run {} in {:.1f}s (total {:.1f}s)", run_id,
               SecondsSince(step), SecondsSince(total_start));

  co_return ClusteringSummary{run_id, static_cast<std::int64_t>(vertex_count),
                              static_cast<std::int64_t>(edges.size()),
                              community_count, detected->modularity};
}

}  // namespace schools::service

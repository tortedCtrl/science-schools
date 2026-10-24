#pragma once

#include <boost/asio.hpp>
#include <cstdint>

#include "common/error.hpp"
#include "graph/leiden.hpp"
#include "repository/community_repository.hpp"
#include "repository/graph_repository.hpp"

namespace schools::service {

struct ClusteringOptions {
  int max_ru_authors_per_work{50};
  double resolution{1.0};
  int threads{0};
};

struct ClusteringSummary {
  std::int64_t run_id{0};
  std::int64_t vertices{0};
  std::int64_t edges{0};
  std::int32_t communities{0};
  double modularity{0.0};
};

// Pipeline: rebuild edges in SQL -> load graph -> Leiden -> save run.
// The CPU-bound Leiden step runs on `compute_executor` so it never blocks the
// I/O threads.
class ClusteringService {
 public:
  ClusteringService(repository::GraphRepository& graph_repo,
                    repository::CommunityRepository& community_repo,
                    graph::CommunityDetector& detector,
                    boost::asio::any_io_executor compute_executor,
                    ClusteringOptions options)
      : graph_repo_{graph_repo},
        community_repo_{community_repo},
        detector_{detector},
        compute_executor_{std::move(compute_executor)},
        options_{options} {}

  boost::asio::awaitable<Result<ClusteringSummary>> Run();

 private:
  repository::GraphRepository& graph_repo_;
  repository::CommunityRepository& community_repo_;
  graph::CommunityDetector& detector_;
  boost::asio::any_io_executor compute_executor_;
  ClusteringOptions options_;
};

}  // namespace schools::service

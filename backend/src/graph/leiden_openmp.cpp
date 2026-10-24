// The only translation unit that includes the third-party GVE-Leiden headers
// (compiled with -w and excluded from clang-tidy, see CMakeLists.txt).
//
// The calls below mirror upstream main.cxx for: DiGraph<K, None, V>,
// leidenStaticOmp(graph, {repeat}), edgeWeightOmp, modularityByOmp.
// addVertex/addEdge/update come from the same author's graph header; if the
// pinned commit differs, adjust ONLY this file.
//
// This is also the only place where exceptions from third-party code are
// caught and converted into Result<>.
#include <omp.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <span>
#include <vector>

#include "common/error.hpp"
#include "graph/leiden.hpp"
#include "inc/main.hxx"

namespace schools::graph {
namespace {

class LeidenOpenmpDetector final : public CommunityDetector {
 public:
  Result<DetectResult> Detect(std::uint32_t vertex_count,
                              std::span<const Edge> edges,
                              const DetectParams& params) override {
    try {
      using K = std::uint32_t;
      using V = float;

      if (params.threads > 0) {
        omp_set_num_threads(params.threads);
      }

      // The algorithm expects a symmetric graph: add both directions.
      DiGraph<K, None, V> graph;
      for (K u = 0; u < vertex_count; ++u) {
        graph.addVertex(u);
      }
      for (const Edge& e : edges) {
        graph.addEdge(e.u, e.v, e.weight);
        graph.addEdge(e.v, e.u, e.weight);
      }
      graph.update();

      auto result{leidenStaticOmp(graph, {params.repeat})};

      DetectResult out;
      out.membership.assign(result.membership.begin(),
                            result.membership.begin() + vertex_count);
      const double total_weight{edgeWeightOmp(graph) / 2};
      auto community_of{[&](auto u) { return result.membership[u]; }};
      out.modularity = total_weight > 0
                           ? modularityByOmp(graph, community_of, total_weight,
                                             params.resolution)
                           : 0.;
      return out;
    } catch (const std::exception& e) {
      return MakeError(Errc::kInternal, "Leiden failed: {}", e.what());
    }
  }
};

}  // namespace

std::unique_ptr<CommunityDetector> MakeLeidenDetector() {
  return std::make_unique<LeidenOpenmpDetector>();
}

}  // namespace schools::graph

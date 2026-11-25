#include "graph_engine/snap_bigclam_bridge.hpp"

#include <agmfast.h>

#ifdef USE_OPENMP
#include <omp.h>
#endif

#include <exception>
#include <set>

namespace schools::graph::detail {

SnapRunResult RunSnapBigClam(const std::vector<SnapEdge>& edges,
                            int communities, int threads) {
  SnapRunResult result;
  try {
    const PUNGraph graph = TUNGraph::New();
    std::set<std::int32_t> ids;
    for (const auto& edge : edges) {
      ids.insert(edge.source);
      ids.insert(edge.target);
    }
    for (auto id : ids) {
      graph->AddNode(id);
    }
    for (const auto& edge : edges) {
      graph->AddEdge(edge.source, edge.target);
    }

#ifdef USE_OPENMP
    omp_set_num_threads(threads);
#endif

    TAGMFast model(graph, 10, 10);
    model.NeighborComInit(communities);
    constexpr double kThreshold = 0.0001;
    constexpr double kAlpha = 0.05;
    constexpr double kBeta = 0.3;
    if (threads == 1 || graph->GetEdges() < 1000) {
      model.MLEGradAscent(kThreshold, 1000 * graph->GetNodes(), "", kAlpha,
                          kBeta);
    } else {
      // Use the same argument order as examples/bigclam/bigclam.cpp.
      model.MLEGradAscentParallel(kThreshold, 1000, threads, "", kAlpha,
                                  kBeta);
    }

    TVec<TIntV> clusters;
    model.GetCmtyVV(clusters);
    for (int cid = 0; cid < clusters.Len(); ++cid) {
      for (int j = 0; j < clusters[cid].Len(); ++j) {
        result.memberships.push_back({static_cast<int>(clusters[cid][j]), cid});
      }
    }
  } catch (const PExcept& error) {
    result.error = error->GetMsgStr().CStr();
  } catch (const std::exception& error) {
    result.error = error.what();
  }
  return result;
}

}  // namespace schools::graph::detail

#include "graph_engine/bigclam_wrapper.hpp"
#include "graph_engine/snap_bigclam_bridge.hpp"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <limits>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace schools::graph {

Result<std::vector<domain::Membership>> BigClamWrapper::Run(
    std::span<const domain::Edge> edges, int communities, int threads) {
  if (communities <= 0 || threads <= 0) {
    return MakeError(Errc::kInvalidArgument,
                     "communities and threads must be positive");
  }
  if (edges.empty()) {
    return MakeError(Errc::kInvalidArgument,
                     "coauthorship graph has no edges");
  }

  // Validate the entire input before invoking SNAP, which uses assertions for
  // some invalid graph operations. The original graph is undirected and
  // unweighted for BigCLAM, just like the old executable-based integration.
  std::set<std::int32_t> ids;
  std::set<std::pair<std::int32_t, std::int32_t>> unique_edges;
  for (const domain::Edge& edge : edges) {
    if (edge.source < 0 || edge.target < 0 || edge.source == edge.target ||
        edge.weight <= 0) {
      return MakeError(Errc::kInvalidArgument,
                       "invalid edge {} - {} (weight {})", edge.source,
                       edge.target, edge.weight);
    }
    ids.insert(edge.source);
    ids.insert(edge.target);
    unique_edges.emplace(std::min(edge.source, edge.target),
                         std::max(edge.source, edge.target));
  }
  if (ids.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return MakeError(Errc::kInvalidArgument, "too many BigCLAM vertices");
  }

  try {
    // Convert edges to the bridge format.
    std::vector<detail::SnapEdge> input;
    input.reserve(unique_edges.size());

    for (const auto& [source, target] : unique_edges) {
        input.push_back({source, target});
    }

    // Run SNAP through the C++17 adapter.
    auto result = detail::RunSnapBigClam(
        input, communities, threads
    );

    if (!result.error.empty()) {
        return MakeError(
            Errc::kInternal,
            "SNAP BigCLAM failed: {}",
            result.error
        );
    }

    // Convert SNAP results to our domain model.
    std::vector<domain::Membership> memberships;
    memberships.reserve(result.memberships.size());

    for (const auto& membership : result.memberships) {
        if (!ids.contains(membership.author_id)) {
            return MakeError(
                Errc::kParse,
                "SNAP BigCLAM returned unknown author {}",
                membership.author_id
            );
        }

      memberships.push_back({
          .author_id = membership.author_id,
          .community_id = membership.community_id
      });
    }
    if (memberships.empty()) {
        return MakeError(
            Errc::kParse,
            "BigCLAM returned no valid memberships"
        );
      }
    return memberships;
  } catch (const std::exception& error) {
    return MakeError(Errc::kInternal, "BigCLAM failed: {}", error.what());
  }
}

}  // namespace schools::graph

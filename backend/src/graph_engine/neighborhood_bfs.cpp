#include "graph_engine/neighborhood_bfs.hpp"

#include <cstdint>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace schools::graph {

domain::GraphData NeighborhoodBfs(const domain::GraphData& full,
                                  std::int32_t author_id, int depth,
                                  std::size_t cap) {
  domain::GraphData result;
  if (depth < 0 || cap == 0) return result;

  std::unordered_set<std::int32_t> existing;
  existing.reserve(full.nodes.size());
  for (const auto& node : full.nodes) existing.insert(node.id);
  if (!existing.contains(author_id)) return result;

  std::unordered_map<std::int32_t, std::vector<std::int32_t>> adjacency;
  adjacency.reserve(existing.size());
  for (const auto& edge : full.links) {
    if (edge.source == edge.target || !existing.contains(edge.source) ||
        !existing.contains(edge.target)) continue;
    adjacency[edge.source].push_back(edge.target);
    adjacency[edge.target].push_back(edge.source);
  }

  std::unordered_set<std::int32_t> visited{author_id};
  std::queue<std::pair<std::int32_t, int>> q;
  q.push({author_id, 0});

  while (!q.empty() && visited.size() < cap) {
    const auto [current, distance] = q.front();
    q.pop();
    if (distance >= depth) continue;
    const auto it = adjacency.find(current);
    if (it == adjacency.end()) continue;
    for (auto next : it->second) {
      if (visited.size() == cap) break;
      if (visited.insert(next).second) q.push({next, distance + 1});
    }
  }

  result.nodes.reserve(visited.size());
  for (const auto& node : full.nodes)
    if (visited.contains(node.id)) result.nodes.push_back(node);
  for (const auto& edge : full.links)
    if (visited.contains(edge.source) && visited.contains(edge.target))
      result.links.push_back(edge);
  for (const auto& membership : full.memberships)
    if (visited.contains(membership.author_id))
      result.memberships.push_back(membership);
  return result;
}

}  // namespace schools::graph

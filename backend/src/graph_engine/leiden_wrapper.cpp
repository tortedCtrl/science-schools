#include "graph_engine/leiden_wrapper.hpp"

#include <spdlog/spdlog.h>

#include <Graph.hxx>
#include <algorithm>
#include <cstdint>
#include <leiden.hxx>
#include <span>
#include <vector>

namespace schools::graph {

std::vector<std::uint32_t> LeidenWrapper::RunLeiden(
    std::span<const CoauthorshipEdge> input_edges, double gamma) {
  if (input_edges.empty()) {
    return {};
  }

  std::uint32_t max_vertex_id{0};
  for (const CoauthorshipEdge& edge : input_edges) {
    max_vertex_id = std::max({max_vertex_id, edge.source_id, edge.target_id});
  }
  const std::uint32_t num_vertices = max_vertex_id + 1;

  // The library indexes vertices densely, so create every vertex up front.
  DiGraph<std::uint32_t, None, float> graph;
  for (std::uint32_t vertex = 0; vertex < num_vertices; ++vertex) {
    graph.addVertex(vertex);
  }

  // The graph is undirected; add both directions explicitly.
  for (const CoauthorshipEdge& edge : input_edges) {
    const auto weight = static_cast<float>(edge.weight);
    graph.addEdge(edge.source_id, edge.target_id, weight);
    graph.addEdge(edge.target_id, edge.source_id, weight);
  }
  graph.update();

  spdlog::debug("Leiden graph: {} vertices, {} edges", graph.order(),
                graph.size());

  LeidenOptions options;
  options.resolution = static_cast<float>(gamma);
  options.repeat = 1;

  const auto result = leidenStatic(graph, options);
  std::vector<std::uint32_t> communities(num_vertices);
  for (std::uint32_t vertex = 0; vertex < num_vertices; ++vertex) {
    communities[vertex] = result.membership[vertex];
  }
  return communities;
}

}  // namespace schools::graph

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace schools::graph {

struct CoauthorshipEdge {
  std::uint32_t source_id{0};
  std::uint32_t target_id{0};
  std::int32_t weight{0};
};

class LeidenWrapper {
 public:
  static std::vector<std::uint32_t> RunLeiden(
      std::span<const CoauthorshipEdge> input_edges, double gamma);
};

}  // namespace schools::graph

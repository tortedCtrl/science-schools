#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "common/error.hpp"

namespace schools::graph {

// Undirected weighted edge between dense vertex indices [0, vertex_count).
struct Edge {
  std::uint32_t u{0};
  std::uint32_t v{0};
  float weight{0.0F};
};

struct DetectParams {
  double resolution{1.0};
  int threads{0};  // 0 = OpenMP default
  int repeat{1};
};

struct DetectResult {
  std::vector<std::uint32_t> membership;  // size == vertex_count
  double modularity{0.0};
};

// Abstraction over the community detection engine, so services never depend on
// the third-party headers. Implementations must not let exceptions escape.
class CommunityDetector {
 public:
  virtual ~CommunityDetector() = default;
  CommunityDetector() = default;
  CommunityDetector(const CommunityDetector&) = delete;
  CommunityDetector& operator=(const CommunityDetector&) = delete;

  virtual Result<DetectResult> Detect(std::uint32_t vertex_count,
                                      std::span<const Edge> edges,
                                      const DetectParams& params) = 0;
};

// GVE-Leiden (puzzlef/leiden-communities-openmp) implementation.
std::unique_ptr<CommunityDetector> MakeLeidenDetector();

}  // namespace schools::graph

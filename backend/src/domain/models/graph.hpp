#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schools::domain {

struct Edge {
  std::int32_t source{0};
  std::int32_t target{0};
  std::int32_t weight{0};
};

}
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schools::domain {

struct IngestResult {
  std::size_t authors{0};
  std::size_t works{0};
  std::size_t edges{0};
  std::size_t articles{0};
  std::size_t communities{0};
};
}
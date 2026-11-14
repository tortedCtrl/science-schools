#pragma once

#include <cstddef>
#include <ranges>
#include <span>
#include <vector>

namespace schools::repository {

inline constexpr std::size_t kBulkChunkSize = 2000;

// SOCI needs an lvalue vector for soci::use(), so each chunk is materialized.
template <typename T>
std::vector<T> Slice(const std::vector<T>& values, std::size_t begin,
                     std::size_t end) {
  return std::span{values}.subspan(begin, end - begin) |
         std::ranges::to<std::vector<T>>();
}

}  // namespace schools::repository

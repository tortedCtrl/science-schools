#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "domain/models/article.hpp"
namespace schools::domain {
struct ArticleTopicInput {
  Article article;
  std::vector<std::int32_t> author_ids;
};
}
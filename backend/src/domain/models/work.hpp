#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "domain/models/article_topic.hpp"

namespace schools::domain {

struct Work {
  std::string openalex_id;
  std::string title;
  std::int32_t year{0};
  std::vector<std::string> author_ids;
  std::vector<ArticleTopic> topics;
};
}
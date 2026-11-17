#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schools::domain {

struct Author {
  std::string openalex_id;
  std::string name;
  std::string institution_name;
};

struct AuthorArticle {
  std::int32_t author_id{0};
  std::string article_id;
};
}
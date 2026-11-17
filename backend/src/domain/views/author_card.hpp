#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "domain/models/article.hpp"
#include "domain/views/author_summary.hpp"

namespace schools::domain {

struct AuthorCard {
  AuthorSummary author;
  std::vector<Article> articles;
};
}
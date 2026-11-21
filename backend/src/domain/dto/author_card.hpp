#pragma once

#include <vector>
#include "domain/db/article.hpp"
#include "domain/dto/author_summary.hpp"

namespace schools::domain {

struct AuthorCard {
    AuthorSummary author;
    std::vector<Article> articles;
};

}  // namespace schools::domain
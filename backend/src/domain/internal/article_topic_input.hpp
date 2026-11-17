#pragma once

#include <vector>
#include "domain/db/article.hpp"
#include "domain/common/types.hpp"

namespace schools::domain {

struct ArticleTopicInput {
    Article article;
    std::vector<AuthorId> author_ids;
};

}  // namespace schools::domain
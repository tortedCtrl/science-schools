#pragma once

#include <string>
#include "domain/common/types.hpp"

namespace schools::domain {

struct AuthorArticle {
    AuthorId author_id{0};
    ArticleId article_id;
};

}  // namespace schools::domain
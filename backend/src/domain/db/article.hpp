#pragma once

#include <string>
#include <vector>
#include "domain/common/types.hpp"
#include "domain/common/topic_hierarchy.hpp"
#include "domain/db/article_topic.hpp"

namespace schools::domain {

struct Article {
    ArticleId openalex_id;
    std::string title;
    std::int32_t year{0};
    std::vector<ArticleTopic> topics;
};

}  // namespace schools::domain
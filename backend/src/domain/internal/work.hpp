#pragma once

#include <string>
#include <vector>
#include "domain/common/types.hpp"
#include "domain/common/topic_hierarchy.hpp"

namespace schools::domain {

struct Work {
    ArticleId openalex_id;
    std::string title;
    std::int32_t year{0};
    std::vector<ArticleId> author_ids;
    std::vector<ArticleTopic> topics;
};

}  // namespace schools::domain
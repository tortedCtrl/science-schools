#pragma once

#include <string>
#include "domain/common/types.hpp"
#include "domain/common/topic_hierarchy.hpp"

namespace schools::domain {

struct ArticleTopic {
    //ArticleId article_id;
    std::string topic_id;
    std::string name;
    double score{0.0};
    bool is_primary{false};
    TopicHierarchy subfield;
    TopicHierarchy field;
    TopicHierarchy domain;
};

}  // namespace schools::domain
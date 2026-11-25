#pragma once

#include <cstdint>
#include <string>

namespace schools::domain {

struct TopicInfo {
    std::string topic_id;
    std::string topic_name;
    std::int64_t article_count{0};
};

}  // namespace schools::domain
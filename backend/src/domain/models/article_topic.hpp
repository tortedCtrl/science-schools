#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schools::domain {
struct ArticleTopic {
    std::string topic_id;
    std::string name;
    double score{0.0};

    std::string subfield_id;
    std::string subfield_name;

    std::string field_id;
    std::string field_name;

    std::string domain_id;
    std::string domain_name;

    bool is_primary{false};
};
}
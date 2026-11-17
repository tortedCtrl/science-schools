#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schools::domain {
struct Membership {
  std::int64_t run_id{0};
  std::int32_t author_id{0};
  std::int32_t community_id{0};
};

struct CommunityTopic {
  std::int64_t run_id{0};
  std::string algorithm;
  std::int32_t community_id{0};
  std::string topic_id;
  std::string topic_name;
  double weight{0.0};
  std::int32_t article_count{0};
};

enum class CommunityAlgorithm { kLeiden, kBigClam };
 
    
}
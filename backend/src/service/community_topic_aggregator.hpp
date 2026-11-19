#pragma once

#include <span>
#include <vector>

#include "domain/models.hpp"

namespace schools::service {

std::vector<domain::CommunityTopic> ComputeCommunityTopics(
    std::span<const domain::Membership> memberships,
    std::span<const domain::ArticleTopicInput> articles,
    domain::CommunityAlgorithm algorithm);

}  // namespace schools::service

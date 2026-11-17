#pragma once

#include "domain/common/types.hpp"

namespace schools::domain {

struct Edge {
    AuthorId source{0};
    AuthorId target{0};
    Weight weight{0};
};

}  // namespace schools::domain
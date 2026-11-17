#pragma once

#include <string>
#include "domain/common/types.hpp"

namespace schools::domain {

struct Author {
    std::string openalex_id;
    std::string name;
    std::string institution_name;
};

}  // namespace schools::domain
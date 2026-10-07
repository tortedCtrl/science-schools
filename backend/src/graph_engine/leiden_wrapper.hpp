#pragma once

#include <vector>
#include <cstdint>

struct CoauthorshipEdge {
    uint32_t source_id;
    uint32_t target_id;
    int weight;
};

class LeidenWrapper {
public:
    static std::vector<uint32_t> run_leiden(const std::vector<CoauthorshipEdge>& input_edges, double gamma);
};

#include "graph_engine/leiden_wrapper.hpp"
#include "database/db_client.hpp"
#include <iostream>
#include <vector>
#include <string>

int main() {
    // сейчас код не скомпилируется нет подключения к базам данных
    std::string connection_string = "postgresql://postgres:secret@localhost:5432/science_schools";
    double gamma = 0.05;
    
    DBClient db(connection_string);
    std::vector<CoauthorshipEdge> edges = db.get_coauthorship_edges();

    if (edges.empty()) {
        std::cerr << "Error: Coauthorship graph is empty." << std::endl;
        return 1;
    }

    std::vector<uint32_t> communities = LeidenWrapper::run_leiden(edges, gamma);
    db.update_communities(communities);

    std::cout << "Community detection completed successfully." << std::endl;
    return 0;
}

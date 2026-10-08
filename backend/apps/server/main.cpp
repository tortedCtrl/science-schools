#include "database/db_client.hpp"
#include "api/openalex_client.hpp"
#include "graph_engine/leiden_wrapper.hpp"
#include <iostream>
#include "nlohmann/json.hpp"
int main() {
    try {
        std::cout << "1. Creating DBClient..." << std::endl;
        std::string connection_string =
            "host=127.0.0.1 "
            "port=5433 "
            "dbname=science_db "
            "user=science_user "
            "password=science_password";
        DBClient db(connection_string);

        std::cout << "2. DB connected" << std::endl;

        OpenAlexClient openalex;

        std::cout << "3. Starting OpenAlex import" << std::endl;
        openalex.loadDataBase(db, 1000);

        std::cout << "4. OpenAlex import finished" << std::endl;

        auto edges = db.get_coauthorship_edges();

        std::cout << "5. Edges: "
                  << edges.size()
                  << std::endl;

        if (edges.empty()) {
            std::cout << "Graph is empty" << std::endl;
            return 1;
        }

        double gamma = 1.0;

        std::cout << "6. Starting Leiden" << std::endl;

        auto communities =
            LeidenWrapper::run_leiden(edges, gamma);

        std::cout << "7. Communities calculated: "
                  << communities.size()
                  << std::endl;

        db.update_communities(communities);

        std::cout << "8. Communities saved" << std::endl;
    }
    catch (const soci::soci_error& e) {
        std::cerr
            << "SOCI ERROR: "
            << e.what()
            << std::endl;

        return 1;
    }
    catch (const nlohmann::json::exception& e) {
        std::cerr
            << "JSON ERROR: "
            << e.what()
            << std::endl;

        return 1;
    }
    catch (const std::exception& e) {
        std::cerr
            << "ERROR: "
            << e.what()
            << std::endl;

        return 1;
    }
}
#include "database/db_client.hpp"
#include "api/openalex_client.hpp"
#include "graph_engine/leiden_wrapper.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include "nlohmann/json.hpp"

namespace {
std::string requiredConninfoValue(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        throw std::runtime_error(std::string("Required environment variable is not set: ") + name);
    }

    std::string escaped = "'";
    for (const char* ch = value; *ch != '\0'; ++ch) {
        if (*ch == '\\' || *ch == '\'') {
            escaped += '\\';
        }
        escaped += *ch;
    }
    escaped += '\'';
    return escaped;
}
}

int main() {
    try {
        std::cout << "1. Creating DBClient..." << std::endl;

        const char* backendInDocker = std::getenv("BACKEND_IN_DOCKER");
        const char* postgresPortEnv =
            backendInDocker != nullptr && std::string_view(backendInDocker) == "true"
                ? "POSTGRES_PORT"
                : "POSTGRES_PUBLISHED_PORT";

        std::string connection_string = // вынести логику
            "host=" + requiredConninfoValue("POSTGRES_HOST") +
            " port=" + requiredConninfoValue(postgresPortEnv) +
            " dbname=" + requiredConninfoValue("POSTGRES_DB") +
            " user=" + requiredConninfoValue("POSTGRES_USER") +
            " password=" + requiredConninfoValue("POSTGRES_PASSWORD");

        DBClient db(connection_string);

        std::cout << "2. DB connected" << std::endl;

        OpenAlexClient openalex;

        std::cout << "3. Starting OpenAlex import" << std::endl;
        openalex.loadDataBase(db, 10);

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
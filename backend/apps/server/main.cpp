#include "database/db_client.hpp"
#include "api/openalex_client.hpp"
#include "graph_engine/leiden_wrapper.hpp"
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include "nlohmann/json.hpp"
#include <httplib.h>
#include "routes.hpp"
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

        httplib::Server server;

        register_routes(server, db);

        std::cout << "Server started on port 8080\n";

        server.listen("0.0.0.0", 8080);
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
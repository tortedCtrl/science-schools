#include "database/db_client.hpp"
#include "api/openalex_client.hpp"
#include "graph_engine/leiden_wrapper.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include "nlohmann/json.hpp"
#include <httplib.h>
#include "routes.hpp"

namespace {
std::string requiredEnvironmentValue(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        throw std::runtime_error(std::string("Required environment variable is not set: ") + name);
    }
    return value;
}
}

int main() {
    try {
        std::cout << "1. Creating DBClient..." << std::endl;
        DBClient db(requiredEnvironmentValue("ConnectionStrings__DefaultConnection"));

        OpenAlexClient client;
        client.loadDataBase(db, 10);

        httplib::Server server;
        register_routes(server, db);

        const std::string host = requiredEnvironmentValue("BACKEND_HOST");
        const int port = std::stoi(requiredEnvironmentValue("BACKEND_PORT"));

        std::cout << "DEBUG: Host is '" << host << "', Port is " << port << std::endl;

        const bool status = server.listen(host, port);
        std::cout << status << std::endl;
        if (!status) {
            return 1;
        }
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

    return 0;
}

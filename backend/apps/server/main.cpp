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

void setCorsHeaders(httplib::Response& res, const std::string& origin) {
    res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Accept");
}
}

int main() {
    try {
        std::cout << "1. Creating DBClient..." << std::endl;
        DBClient db(requiredEnvironmentValue("ConnectionStrings__DefaultConnection"));

        // ===== ЗАГРУЗКА ДАННЫХ ИЗ OPENALEX =====
        // Раскомментируйте, если нужно загрузить данные при старте.
        // После первой загрузки лучше держать закомментированным,
        // чтобы сервер стартовал мгновенно.

        // {
        //     std::cout << "2. Loading data from OpenAlex..." << std::endl;
        //     OpenAlexClient loader;
        //     loader.loadDataBase(db, 100);
        //     std::cout << "   Data loaded." << std::endl;
        // }


        httplib::Server server;
        register_routes(server, db);

        server.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
            std::string origin = req.get_header_value("Origin");
            if (origin == "http://127.0.0.1:" + requiredEnvironmentValue("FRONTEND_PORT")
                || origin == "http://localhost:" + requiredEnvironmentValue("FRONTEND_PORT")) {
                res.set_header("Access-Control-Allow-Origin", origin);
            }
            setCorsHeaders(res, origin);
            });

        const std::string host = requiredEnvironmentValue("BACKEND_HOST");
        const int port = std::stoi(requiredEnvironmentValue("BACKEND_PORT"));

        std::cout << "DEBUG: Host is '" << host << "', Port is " << port << std::endl;

        bool status = server.listen(host, port);
        std::cout << status << std::endl;
    }
    catch (const soci::soci_error& e) {
        std::cerr << "SOCI ERROR: " << e.what() << std::endl;
        return 1;
    }
    catch (const nlohmann::json::exception& e) {
        std::cerr << "JSON ERROR: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
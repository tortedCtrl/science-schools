#include "routes.hpp"
#include "database/db_client.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::string frontendOrigin() {
    const char* port = std::getenv("FRONTEND_PORT");
    if (port == nullptr || port[0] == '\0') {
        throw std::runtime_error("Required environment variable is not set: FRONTEND_PORT");
    }
    return "http://localhost:" + std::string(port);
}

void setCorsHeaders(httplib::Response& res, const std::string& origin) {
    res.set_header("Access-Control-Allow-Origin", origin);
    res.set_header("Access-Control-Allow-Methods", "GET, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Accept");
}
}

void register_routes(
    httplib::Server& server,
    DBClient& db
) {
    const std::string origin = frontendOrigin();

    server.Options("/authors", [&](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res, origin);
        res.status = 204;
    });

    server.Options(R"(/authors/(\d+))", [&](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res, origin);
        res.status = 204;
    });

    server.Get("/authors", [&](const httplib::Request&, httplib::Response& res) {
        setCorsHeaders(res, origin);
        try {
            const std::string json = db.get_all_authors_json();
            res.set_content(json, "application/json");
            res.status = 200;
        }
        catch (const std::exception& e) {
            res.status = 500;
            std::cerr << e.what() << std::endl;
            res.set_content(R"({"error":"internal server error"})", "application/json");
        }
    });

    server.Get(R"(/authors/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        setCorsHeaders(res, origin);
        try {
            const uint32_t id = static_cast<uint32_t>(std::stoul(req.matches[1].str()));
            const std::string json = db.get_author_card_json(id);
            res.set_content(json, "application/json");
            res.status = 200;
        }
        catch (const std::exception& e) {
            res.status = 500;
            std::cerr << e.what() << std::endl;
            res.set_content(R"({"error":"internal server error"})", "application/json");
        }
    });
}

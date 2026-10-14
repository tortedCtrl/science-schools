#include "routes.hpp"
#include "database/db_client.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

void register_routes(
    httplib::Server& server,
    DBClient& db
) {
    server.Options("/authors", [&](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
        });

    server.Options(R"(/authors/(\d+))", [&](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
        });

    server.Options("/graph", [&](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
        });

    server.Get("/authors", [&](const httplib::Request&, httplib::Response& res) {
        try {
            const std::string json = db.get_all_authors_json();
            res.set_content(json, "application/json");
            res.status = 200;
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            res.status = 500;
            res.set_content(R"({"error":"internal server error"})", "application/json");
        }
        });

    server.Get(R"(/authors/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            const uint32_t id = static_cast<uint32_t>(std::stoul(req.matches[1].str()));
            const std::string json = db.get_author_card_json(id);
            res.set_content(json, "application/json");
            res.status = 200;
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            res.status = 500;
            std::cerr << e.what() << std::endl;
            res.set_content(R"({"error":"internal server error"})", "application/json");
        }
        });

    server.Get("/edges", [&](const httplib::Request&, httplib::Response& res) {
        try {
            std::string json = db.get_all_edges_json();
            res.set_content(json, "application/json");
            res.status = 200;
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            res.status = 500;
            res.set_content(R"({"error":"internal server error"})", "application/json");
        }
        }
    );

    server.Get("/graph", [&](const httplib::Request&, httplib::Response& res) {
        try {
            std::string json = db.get_graph_json();
            res.set_content(json, "application/json");
            res.status = 200;
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            res.status = 500;
            res.set_content(R"({"error":"internal server error"})", "application/json");
        }
        }
    );
}
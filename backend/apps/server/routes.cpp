#include "routes.hpp"
#include "database/db_client.hpp"

#include <string>

void register_routes(
    httplib::Server& server,
    DBClient& db
) {
    server.Get("/authors", [&](const httplib::Request&, httplib::Response& res) {
            try {
                std::string json = db.get_all_authors_json();
                res.set_content(json, "application/json");
                res.status = 200;
            }
            catch (const std::exception&) {
                res.status = 500;
                res.set_content(R"({"error":"internal server error"})", "application/json");
            }
        }
    );

    server.Get(R"(/authors/(\d+))", [&](const httplib::Request& req, httplib::Response& res) {
            try {
                uint32_t id = static_cast<uint32_t>(std::stoul(req.matches[1].str()));
                std::string json = db.get_author_card_json(id);
                res.set_content(json, "application/json");
                res.status = 200;
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(R"({"error":"internal server error"})", "application/json");
            }
        }
    );
}
#include "api/routes.hpp"

#include <spdlog/spdlog.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <system_error>

#include "common/error.hpp"

namespace schools::api {
namespace {

constexpr const char* kJsonContentType = "application/json";

void SendJson(httplib::Response& response, const std::string& body,
              int status = 200) {
  response.status = status;
  response.set_content(body, kJsonContentType);
}

int HttpStatus(Errc code) {
  switch (code) {
    case Errc::kInvalidArgument:
      return 400;
    case Errc::kNotFound:
      return 404;
    case Errc::kUnavailable:
      return 503;
    default:
      return 500;
  }
}

void SendError(httplib::Response& response, const Error& error) {
  spdlog::error("API error: {}", error.message);
  SendJson(response, R"({"error":"internal server error"})",
           HttpStatus(error.code));
}

void SendBody(httplib::Response& response, const Result<std::string>& body) {
  if (!body) {
    SendError(response, body.error());
    return;
  }
  SendJson(response, *body);
}

}  // namespace

void RegisterRoutes(httplib::Server& server, service::QueryService& query) {
  server.Get("/api/v1/health",
             [](const httplib::Request&, httplib::Response& response) {
               SendJson(response, R"({"status":"ok"})");
             });

  server.Options("/authors",
                 [](const httplib::Request&, httplib::Response& response) {
                   response.status = 204;
                 });
  server.Options(R"(/authors/(\d+))",
                 [](const httplib::Request&, httplib::Response& response) {
                   response.status = 204;
                 });
  server.Options("/edges",
                 [](const httplib::Request&, httplib::Response& response) {
                   response.status = 204;
                 });
  server.Options("/graph",
                 [](const httplib::Request&, httplib::Response& response) {
                   response.status = 204;
                 });

  server.Get("/authors",
             [&query](const httplib::Request&, httplib::Response& response) {
               SendBody(response, query.ListAuthorsJson());
             });

  server.Get(R"(/authors/(\d+))", [&query](const httplib::Request& request,
                                           httplib::Response& response) {
    const std::string text = request.matches[1].str();
    std::int32_t id{0};
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto [ptr, error] = std::from_chars(begin, end, id);
    if (error != std::errc{} || ptr != end) {
      SendJson(response, R"({"error":"invalid author id"})", 400);
      return;
    }
    SendBody(response, query.GetAuthorCardJson(id));
  });

  server.Get("/edges",
             [&query](const httplib::Request&, httplib::Response& response) {
               SendBody(response, query.ListEdgesJson());
             });

  server.Get("/graph",
             [&query](const httplib::Request&, httplib::Response& response) {
               SendBody(response, query.GetGraphJson());
             });
}

}  // namespace schools::api

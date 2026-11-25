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
  server.Options(R"(/communities/(leiden|bigclam))",
                 [](const httplib::Request&, httplib::Response& response) {
                   response.status = 204;
                 });
  server.Options(R"(/communities/(leiden|bigclam)/(\d+)/topics)",
                 [](const httplib::Request&, httplib::Response& response) {
                   response.status = 204;
                 });

  server.Options("/topics",
                [](const httplib::Request&, httplib::Response& response) {
                  response.status = 204;
                });
  server.Options("/graph/neighborhood",
                [](const httplib::Request&,
                  httplib::Response& response) {
                  response.status = 204;
                });
  server.Options(R"(/clusters/(leiden|bigclam))",
                [](const httplib::Request&, httplib::Response& response) {
                  response.status = 204;
                });

  server.Options(R"(/clusters/(leiden|bigclam)/topics)",
               [](const httplib::Request&, httplib::Response& response) {
                 response.status = 204;
               });

  server.Options(R"(/clusters/(leiden|bigclam)/\d+)",
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
             [&query](const httplib::Request& request,
                      httplib::Response& response) {
               const std::string algorithm =
                   request.has_param("algorithm")
                       ? request.get_param_value("algorithm")
                       : "leiden";
               if (algorithm != "leiden" && algorithm != "bigclam") {
                 SendJson(response,
                          R"({"error":"algorithm must be leiden or bigclam"})",
                          400);
                 return;
               }
               SendBody(response, query.GetGraphJson(algorithm));
             });
  
  server.Get(
      "/graph/neighborhood",
      [&query](const httplib::Request& request,
              httplib::Response& response) {

        auto parse_number = [&](const char* name,
                                std::int32_t& value) -> bool {
          if (!request.has_param(name)) {
            return false;
          }

          const std::string text = request.get_param_value(name);

          const auto [ptr, error] = std::from_chars(
              text.data(),
              text.data() + text.size(),
              value
          );

          return error == std::errc{} &&
                ptr == text.data() + text.size();
        };

        std::int32_t author_id = 0;
        std::int32_t depth = 0;
        std::int32_t v_cap = 0;

        const std::string algorithm =
            request.has_param("algorithm")
                ? request.get_param_value("algorithm")
                : "leiden";

        if ((algorithm != "leiden" &&
            algorithm != "bigclam") ||
            !parse_number("author_id", author_id) ||
            !parse_number("depth", depth) ||
            !parse_number("v_cap", v_cap) ||
            author_id < 0 ||
            depth < 0 || depth > 3 ||
            v_cap < 1 || v_cap > 1000) {

          SendJson(
              response,
              R"({"error":"invalid neighborhood parameters"})",
              400
          );
          return;
        }

        SendBody(
            response,
            query.GetNeighborhoodJson(
                algorithm,
                author_id,
                depth,
                static_cast<std::size_t>(v_cap)
            )
        );
      }
  );

  server.Get(R"(/communities/(leiden|bigclam))",
             [&query](const httplib::Request& request,
                      httplib::Response& response) {
               SendBody(response,
                        query.ListCommunitiesJson(request.matches[1].str()));
             });
  server.Get(R"(/communities/(leiden|bigclam)/(\d+)/topics)",
             [&query](const httplib::Request& request,
                      httplib::Response& response) {
               const std::string algorithm = request.matches[1].str();
               const std::string text = request.matches[2].str();
               std::int32_t community_id{0};
               const char* begin = text.data();
               const char* end = begin + text.size();
               const auto [ptr, error] =
                   std::from_chars(begin, end, community_id);
               if (error != std::errc{} || ptr != end || community_id < 0) {
                 SendJson(response,
                          R"({"error":"invalid community id"})", 400);
                 return;
               }
               SendBody(response,
                        query.ListCommunityTopicsJson(algorithm, community_id));
             });
  // All available topics.
  server.Get("/topics",
            [&query](const httplib::Request&, httplib::Response& response) {
              SendBody(response, query.ListAllTopicsJson());
            });

  // Cluster metadata.
  server.Get(R"(/clusters/(leiden|bigclam))",
            [&query](const httplib::Request& request,
                      httplib::Response& response) {
              const std::string algorithm = request.matches[1].str();
              SendBody(response, query.ListClusterMetadataJson(algorithm));
            });

  // Clusters containing a particular topic.
  server.Get(R"(/clusters/(leiden|bigclam)/topics)",
            [&query](const httplib::Request& request,
                      httplib::Response& response) {
              const std::string algorithm = request.matches[1].str();

              if (!request.has_param("topic_id")) {
                SendJson(response,
                          R"({"error":"topic_id parameter is required"})",
                          400);
                return;
              }

              const std::string topic_id =
                  request.get_param_value("topic_id");

              SendBody(response,
                        query.ListClustersByTopicJson(algorithm, topic_id));
            });

  // Full cluster content.
  server.Get(R"(/clusters/(leiden|bigclam)/(\d+))",
            [&query](const httplib::Request& request,
                      httplib::Response& response) {
              const std::string algorithm = request.matches[1].str();
              const std::string text = request.matches[2].str();

              std::int32_t community_id{0};
              const char* begin = text.data();
              const char* end = begin + text.size();

              const auto [ptr, error] =
                  std::from_chars(begin, end, community_id);

              if (error != std::errc{} || ptr != end) {
                SendJson(response,
                          R"({"error":"invalid community id"})", 400);
                return;
              }

              SendBody(response,
                        query.GetClusterContentJson(algorithm, community_id));
            });         
}

}  // namespace schools::api

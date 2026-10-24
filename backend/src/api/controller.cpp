#include "api/controller.hpp"

#include <boost/json.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "common/parse.hpp"

namespace schools::api {
namespace json = boost::json;
namespace asio = boost::asio;

namespace {

// Missing/empty parameter -> nullopt; malformed -> error.
template <typename T>
Result<std::optional<T>> QueryNumber(const Request& request,
                                     const std::string& name) {
  const auto it{request.query.find(name)};
  if (it == request.query.end() || it->second.empty()) {
    return std::optional<T>{};
  }
  auto value{ParseNumber<T>(it->second, name)};
  if (!value) {
    return std::unexpected(value.error());
  }
  return std::optional<T>{*value};
}

template <typename T>
Result<T> QueryNumberOr(const Request& request, const std::string& name,
                        T fallback) {
  auto value{QueryNumber<T>(request, name)};
  if (!value) {
    return std::unexpected(value.error());
  }
  return value->value_or(fallback);
}

Response JsonResponse(const json::value& value) {
  return {200, json::serialize(value), "application/json; charset=utf-8"};
}

json::value OptionalString(const std::optional<std::string>& value) {
  return value ? json::value(*value) : json::value(nullptr);
}

json::array FieldsToJson(const std::vector<domain::FieldCount>& fields) {
  json::array result;
  for (const auto& field : fields) {
    result.push_back(json::object{{"id", field.id},
                                  {"name", field.name},
                                  {"works_count", field.works_count}});
  }
  return result;
}

}  // namespace

void ApiController::Register(Router& router) const {
  router.Add("GET", "/api/v1/health",
             [](Request, PathParams) -> asio::awaitable<Response> {
               co_return JsonResponse(json::object{{"status", "ok"}});
             });
  router.Add("GET", "/api/v1/authors/{id}/community",
             [this](Request const&, PathParams params) {
               return GetAuthorCommunity(std::move(params));
             });
  router.Add("GET", "/api/v1/communities", [this](Request request, PathParams) {
    return ListCommunities(std::move(request));
  });
  router.Add("GET", "/api/v1/communities/{id}",
             [this](Request request, PathParams params) {
               return GetCommunity(std::move(request), std::move(params));
             });
  router.Add("GET", "/api/v1/graph", [this](Request request, PathParams) {
    return GetGraph(std::move(request));
  });
}

asio::awaitable<Response> ApiController::GetAuthorCommunity(
    PathParams params) const {
  const auto author_id{ParseNumber<std::int64_t>(params["id"], "id")};
  if (!author_id) {
    co_return ErrorResponse(author_id.error());
  }

  const auto result = co_await service_.GetAuthorCommunity(*author_id);
  if (!result) {
    co_return ErrorResponse(result.error());
  }
  co_return JsonResponse(json::object{
      {"author", json::object{{"openalex_id", result->author.openalex_id},
                              {"orcid", OptionalString(result->author.orcid)},
                              {"name", result->author.name}}},
      {"run_id", result->run_id},
      {"community_id", result->community_id},
      {"community_size", result->community_size},
      {"author_fields", FieldsToJson(result->author_fields)},
      {"community_top_fields", FieldsToJson(result->community_top_fields)}});
}

asio::awaitable<Response> ApiController::ListCommunities(
    Request request) const {
  const auto limit{QueryNumberOr<int>(request, "limit", 50)};
  if (!limit) {
    co_return ErrorResponse(limit.error());
  }

  const auto summaries = co_await service_.ListCommunities(*limit);
  if (!summaries) {
    co_return ErrorResponse(summaries.error());
  }
  json::array items;
  for (const auto& summary : *summaries) {
    items.push_back(json::object{{"community_id", summary.community_id},
                                 {"size", summary.size}});
  }
  co_return JsonResponse(json::object{{"communities", std::move(items)}});
}

asio::awaitable<Response> ApiController::GetCommunity(Request request,
                                                      PathParams params) const {
  const auto community_id{ParseNumber<std::int32_t>(params["id"], "id")};
  if (!community_id) {
    co_return ErrorResponse(community_id.error());
  }
  const auto limit{QueryNumberOr<int>(request, "limit", 100)};
  if (!limit) {
    co_return ErrorResponse(limit.error());
  }
  const auto offset{QueryNumberOr<int>(request, "offset", 0)};
  if (!offset) {
    co_return ErrorResponse(offset.error());
  }

  const auto details =
      co_await service_.GetCommunity(*community_id, *limit, *offset);
  if (!details) {
    co_return ErrorResponse(details.error());
  }

  json::array members;
  for (const auto& member : details->members) {
    members.push_back(
        json::object{{"openalex_id", member.author_id},
                     {"name", member.name},
                     {"orcid", OptionalString(member.orcid)},
                     {"weighted_degree", member.weighted_degree}});
  }
  co_return JsonResponse(
      json::object{{"run_id", details->run_id},
                   {"community_id", details->community_id},
                   {"size", details->size},
                   {"top_fields", FieldsToJson(details->top_fields)},
                   {"members", std::move(members)}});
}

asio::awaitable<Response> ApiController::GetGraph(Request request) const {
  const auto community_id{QueryNumber<std::int32_t>(request, "community_id")};
  if (!community_id) {
    co_return ErrorResponse(community_id.error());
  }
  const auto limit{QueryNumberOr<int>(request, "limit", 500)};
  if (!limit) {
    co_return ErrorResponse(limit.error());
  }
  const auto min_weight{QueryNumberOr<float>(request, "min_weight", 0.0F)};
  if (!min_weight) {
    co_return ErrorResponse(min_weight.error());
  }

  const auto slice =
      co_await service_.GetGraph(*community_id, *limit, *min_weight);
  if (!slice) {
    co_return ErrorResponse(slice.error());
  }

  json::array nodes;
  for (const auto& node : slice->nodes) {
    nodes.push_back(json::object{{"id", node.author_id},
                                 {"name", node.name},
                                 {"community_id", node.community_id},
                                 {"weighted_degree", node.weighted_degree}});
  }
  json::array edges;
  for (const auto& edge : slice->edges) {
    edges.push_back(json::object{
        {"source", edge.src}, {"target", edge.dst}, {"weight", edge.weight}});
  }
  co_return JsonResponse(json::object{{"run_id", slice->run_id},
                                      {"truncated", slice->truncated},
                                      {"nodes", std::move(nodes)},
                                      {"edges", std::move(edges)}});
}

}  // namespace schools::api

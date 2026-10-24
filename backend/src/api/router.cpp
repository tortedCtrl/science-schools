#include "api/router.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <boost/json.hpp>
#include <string_view>
#include <utility>

namespace schools::api {
namespace {

std::vector<std::string> SplitPath(std::string_view path) {
  std::vector<std::string> segments;
  while (!path.empty()) {
    if (path.front() == '/') {
      path.remove_prefix(1);
      continue;
    }
    const auto slash{path.find('/')};
    segments.emplace_back(path.substr(0, slash));
    path = slash == std::string_view::npos ? std::string_view{}
                                           : path.substr(slash);
  }
  return segments;
}

Response JsonError(int status, const std::string& message) {
  return {status,
          boost::json::serialize(boost::json::object{{"error", message}}),
          "application/json; charset=utf-8"};
}

bool IsParam(const std::string& segment) {
  return segment.size() > 2 && segment.front() == '{' && segment.back() == '}';
}

}  // namespace

Response ErrorResponse(const Error& error) {
  switch (error.code) {
    case Errc::kInvalidArgument:
      return JsonError(400, error.message);
    case Errc::kNotFound:
      return JsonError(404, error.message);
    case Errc::kUnavailable:
      return JsonError(503, error.message);
    case Errc::kDatabase:
    case Errc::kNetwork:
    case Errc::kParse:
    case Errc::kInternal:
      break;
  }
  spdlog::error("Request failed: {}", error.message);
  return JsonError(500, "Internal server error");
}

void Router::Add(std::string method, const std::string& pattern,
                 Handler handler) {
  routes_.push_back(
      {std::move(method), SplitPath(pattern), std::move(handler)});
}

boost::asio::awaitable<Response> Router::Handle(Request request) const {
  if (request.method == "OPTIONS") {
    // CORS preflight; the CORS headers are added by the HTTP server.
    co_return Response{204, "", "text/plain"};
  }
  const auto segments{SplitPath(request.path)};
  bool path_matched{false};

  for (const Route& route : routes_) {
    if (route.segments.size() != segments.size()) {
      continue;
    }
    PathParams params;
    bool match{true};
    for (std::size_t i = 0; i < segments.size() && match; ++i) {
      if (IsParam(route.segments[i])) {
        params[route.segments[i].substr(1, route.segments[i].size() - 2)] =
            segments[i];
      } else {
        match = route.segments[i] == segments[i];
      }
    }
    if (!match) {
      continue;
    }
    path_matched = true;
    if (route.method != request.method) {
      continue;
    }

    co_return co_await route.handler(std::move(request), std::move(params));
  }
  co_return path_matched ? JsonError(405, "Method not allowed")
                         : JsonError(404, "Route not found");
}

}  // namespace schools::api

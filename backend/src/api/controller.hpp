#pragma once

#include <boost/asio.hpp>

#include "api/http_types.hpp"
#include "api/router.hpp"
#include "service/query_service.hpp"

namespace schools::api {

//   GET /api/v1/health
//   GET /api/v1/authors/{openalex_id}/community
//   GET /api/v1/communities?limit=50
//   GET /api/v1/communities/{community_id}?limit=100&offset=0
//   GET /api/v1/graph?community_id=7&limit=500&min_weight=2

class ApiController {
 public:
  explicit ApiController(const service::QueryService& service)
      : service_{service} {}

  void Register(Router& router) const;

 private:
  boost::asio::awaitable<Response> GetAuthorCommunity(PathParams params) const;
  boost::asio::awaitable<Response> ListCommunities(Request request) const;
  boost::asio::awaitable<Response> GetCommunity(Request request,
                                                PathParams params) const;
  boost::asio::awaitable<Response> GetGraph(Request request) const;

  const service::QueryService& service_;
};

}  // namespace schools::api

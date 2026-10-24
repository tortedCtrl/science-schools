#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <string>
#include <vector>

#include "api/http_types.hpp"
#include "common/error.hpp"

namespace schools::api {

using Handler =
    std::function<boost::asio::awaitable<Response>(Request, PathParams)>;

// Maps an Error to an HTTP response (400/404/503; everything else is a
// generic 500 whose details are only logged).
Response ErrorResponse(const Error& error);

class Router {
 public:
  void Add(std::string method, const std::string& pattern, Handler handler);

  boost::asio::awaitable<Response> Handle(Request request) const;

 private:
  struct Route {
    std::string method;
    std::vector<std::string> segments;
    Handler handler;
  };

  std::vector<Route> routes_;
};

}  // namespace schools::api

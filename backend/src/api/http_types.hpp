#pragma once

#include <string>
#include <unordered_map>

namespace schools::api {

struct Request {
  std::string method;
  std::string path;
  std::unordered_map<std::string, std::string> query;
};

struct Response {
  int status{200};
  std::string body;
  std::string content_type{"application/json; charset=utf-8"};
};

using PathParams = std::unordered_map<std::string, std::string>;

}  // namespace schools::api

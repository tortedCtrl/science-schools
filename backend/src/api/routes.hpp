#pragma once

#include <httplib.h>

#include "service/query_service.hpp"

namespace schools::api {

// Registers the endpoints expected by the existing frontend. The handler code
// only maps HTTP to QueryService; SQL and JSON construction live below it.
void RegisterRoutes(httplib::Server& server, service::QueryService& query);

}  // namespace schools::api

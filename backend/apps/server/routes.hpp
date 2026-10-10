#pragma once
#include <httplib.h>

class DBClient;

void register_routes(
    httplib::Server& server,
    DBClient& db
);
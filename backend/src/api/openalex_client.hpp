#pragma once

#include <string>
class DBClient;

class OpenAlexClient {
public:
    std::string get(const std::string& url);

    void loadDataBase(DBClient& db, int max_authors = 500);
};
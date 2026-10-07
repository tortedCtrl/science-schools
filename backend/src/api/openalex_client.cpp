#include "openalex_client.hpp"

#include <curl/curl.h>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <iostream>

struct RawAuthor {
    std::string openalex_id;
    std::string name;
    std::string institution;
};

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::string url_encode(const std::string& value)
{
    CURL* curl = curl_easy_init();
    char* encoded =curl_easy_escape(curl, value.c_str(), static_cast<int>(value.size()));
    std::string result = encoded;
    curl_free(encoded);
    curl_easy_cleanup(curl);
    return result;
}

std::string OpenAlexClient::get(const std::string& url) {
    CURL* curl = curl_easy_init();

    if (curl == nullptr) {
        throw std::runtime_error(
            "curl_easy_init failed"
        );
    }

    std::string response;

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        url.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        write_callback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &response
    );

    CURLcode result =
        curl_easy_perform(curl);

    if (result != CURLE_OK) {
        std::string error =
            curl_easy_strerror(result);

        curl_easy_cleanup(curl);

        throw std::runtime_error(error);
    }

    curl_easy_cleanup(curl);

    return response;

}
void OpenAlexClient::loadDataBase(DBClient& db, int max_authors) {
    std::vector<RawAuthor> authors;
    std::string cursor = "*";
    while (authors.size() < static_cast<size_t>(max_authors)) {
        std::string url =
            "https://api.openalex.org/authors?"
            "filter=last_known_institutions.country_code:RU"
            "&per_page=100"
            "&cursor=" + url_encode(cursor);
        std::string body = get(url);
        nlohmann::json data = nlohmann::json::parse(body);
        for (const auto& author : data["results"]) {
            RawAuthor a;
            a.openalex_id = author["id"].get<std::string>();
            a.name = author["display_name"].get<std::string>();
            if ( author.contains("last_known_institutions") &&
                !author["last_known_institutions"].empty()) {
                a.institution = author["last_known_institutions"][0]["display_name"].get<std::string>();
            }
            authors.push_back(a);
            if ( authors.size() >= static_cast<size_t>(max_authors)) {
                break;
            }
        }
        if ( data["meta"]["next_cursor"].is_null() ) {
            break;
        }
        cursor = data["meta"]["next_cursor"].get<std::string>();
        
        std::cout << "Downloaded authors: " << authors.size() << '\n';
    }
}

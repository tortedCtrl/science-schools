#include "openalex_client.hpp"
#include "database/db_client.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <iostream>
#include <unordered_set>
#include <algorithm>
struct RawAuthor {
    std::string openalex_id;
    std::string name;
    std::string institution;
};
struct RawArticle {
    std::string openalex_id;
    std::string title;
    int year = 0;
    std::vector<std::string> author_ids;
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
    std::unordered_map<std::string, RawAuthor> authors_by_id;
    for (const auto& author : authors) {
        authors_by_id[author.openalex_id] = author;
    }
    std::unordered_map<std::string, RawArticle> articles_by_id;
    std::unordered_set<std::string> seen_works;
    std::map<std::pair<std::string, std::string>,int> edge_weights;
    const size_t batch_size = 100;
    for (size_t batch_start = 0; batch_start < authors.size(); batch_start += batch_size) {
        std::string author_filter;
        size_t batch_end = (std::min)(batch_start + batch_size, authors.size());

        for (size_t i = batch_start; i < batch_end; ++i) {
            std::string id = authors[i].openalex_id;
            size_t slash = id.find_last_of('/');
            if (slash != std::string::npos) {
                id = id.substr(slash + 1);
            }
            if (!author_filter.empty()) {
                author_filter += "|";
            }
            author_filter += id;
        }

        std::string work_cursor = "*";

        while (true) {
            std::string url = "https://api.openalex.org/works?"
                            "filter=authorships.author.id:" + url_encode(author_filter) +
                            "&select=id,title,publication_year,authorships"
                            "&per_page=100"
                            "&cursor=" + url_encode(work_cursor);

            std::string body = get(url);
            nlohmann::json data = nlohmann::json::parse(body);

            for (const auto& work : data["results"]) {
                if (!work.contains("id") || !work["id"].is_string()) {
                    continue;
                }

                std::string work_id = work["id"].get<std::string>();

                if (!seen_works.insert(work_id).second) {
                    continue;
                }

                RawArticle article;
                article.openalex_id = work_id;

                if (work.contains("title") && work["title"].is_string()) {
                    article.title = work["title"].get<std::string>();
                }

                if (work.contains("publication_year") && work["publication_year"].is_number_integer()) {
                    article.year = work["publication_year"].get<int>();
                }

                std::unordered_set<std::string> selected_authors_set;

                if (work.contains("authorships")) {
                    for (const auto& authorship : work["authorships"]) {
                        if (!authorship.contains("author")) {
                            continue;
                        }

                        const auto& author = authorship["author"];

                        if (!author.contains("id") || !author["id"].is_string()) {
                            continue;
                        }

                        std::string author_id = author["id"].get<std::string>();

                        if (authors_by_id.contains(author_id)) {
                            selected_authors_set.insert(author_id);
                        }
                    }
                }

                article.author_ids.assign(selected_authors_set.begin(), selected_authors_set.end());
                articles_by_id[work_id] = article;

                const auto& work_authors = article.author_ids;

                for (size_t i = 0; i < work_authors.size(); ++i) {
                    for (size_t j = i + 1; j < work_authors.size(); ++j) {
                        std::string a = work_authors[i];
                        std::string b = work_authors[j];

                        if (a > b) {
                            std::swap(a, b);
                        }

                        ++edge_weights[{a, b}];
                    }
                }
            }

            if (data["meta"]["next_cursor"].is_null()) {
                break;
            }

            work_cursor = data["meta"]["next_cursor"].get<std::string>();
        }
    }
    std::unordered_set<std::string> connected_authors;
    for (const auto& [pair, weight] : edge_weights) {
        connected_authors.insert(pair.first);
        connected_authors.insert(pair.second);
    }
    std::unordered_map<std::string, int> dense_id;
    int id = 0;
    for (const auto& author : authors) {
        if (connected_authors.contains(author.openalex_id)) {
            dense_id[author.openalex_id] = id;
            ++id;
        }
    }
    for (const auto& author : authors) {
        auto it = dense_id.find(author.openalex_id);
        if (it == dense_id.end()) {
            continue;
        }
        db.insert_author(
            it->second,
            author.openalex_id,
            author.name,
            author.institution
        );
    }
    for (const auto& [pair, weight] : edge_weights) {
        int source = dense_id.at(pair.first);
        int target = dense_id.at(pair.second);
        if (source > target) {
            std::swap(source, target);
        }
        db.insert_edge(
            source,
            target,
            weight
        );
    }
    for (const auto& [work_id, article] : articles_by_id) {

    bool article_needed = false;

    for (const auto& author_id : article.author_ids) {

        if (dense_id.contains(author_id)) {
            article_needed = true;
            break;
        }
    }

    if (!article_needed) {
        continue;
    }


    db.insert_article(
        article.openalex_id,
        article.title,
        article.year
    );


    for (const auto& author_id : article.author_ids) {

        auto it = dense_id.find(author_id);

        if (it == dense_id.end()) {
            continue;
        }

        db.insert_author_article(
            it->second,
            article.openalex_id
        );
    }
}
}

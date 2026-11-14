#include "openalex/parser.hpp"

#include <algorithm>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace schools::openalex {
namespace {

using Json = nlohmann::json;

std::string GetString(const Json& object, const char* key) {
  if (!object.contains(key) || !object[key].is_string()) {
    return {};
  }
  return object[key].get<std::string>();
}

std::int32_t GetInt(const Json& object, const char* key) {
  if (!object.contains(key) || !object[key].is_number_integer()) {
    return 0;
  }
  return object[key].get<std::int32_t>();
}

Result<Json> ParseRoot(std::string_view body) {
  Json root = Json::parse(body, nullptr, false);
  if (root.is_discarded()) {
    return MakeError(Errc::kParse, "invalid OpenAlex JSON response");
  }
  if (!root.is_object() || !root.contains("results") ||
      !root["results"].is_array()) {
    return MakeError(Errc::kParse, "OpenAlex response has no 'results' array");
  }
  return root;
}

}  // namespace

Result<std::vector<domain::Author>> ParseAuthors(std::string_view body) {
  auto root = ParseRoot(body);
  if (!root) {
    return std::unexpected(root.error());
  }

  std::vector<domain::Author> authors;
  authors.reserve(root->at("results").size());

  for (const Json& item : root->at("results")) {
    if (!item.is_object()) {
      continue;
    }

    domain::Author author{};
    author.openalex_id = GetString(item, "id");
    author.name = GetString(item, "display_name");
    if (item.contains("last_known_institutions") &&
        item["last_known_institutions"].is_array() &&
        !item["last_known_institutions"].empty() &&
        item["last_known_institutions"][0].is_object()) {
      author.institution_name =
          GetString(item["last_known_institutions"][0], "display_name");
    }

    if (!author.openalex_id.empty()) {
      authors.push_back(std::move(author));
    }
  }
  return authors;
}

Result<std::vector<domain::Work>> ParseWorks(
    std::string_view body,
    const std::unordered_set<std::string>& selected_authors) {
  auto root = ParseRoot(body);
  if (!root) {
    return std::unexpected(root.error());
  }

  std::vector<domain::Work> works;
  works.reserve(root->at("results").size());

  for (const Json& item : root->at("results")) {
    if (!item.is_object()) {
      continue;
    }

    domain::Work work{};
    work.openalex_id = GetString(item, "id");
    work.title = GetString(item, "title");
    work.year = GetInt(item, "publication_year");
    if (work.openalex_id.empty()) {
      continue;
    }

    if (item.contains("authorships") && item["authorships"].is_array()) {
      std::unordered_set<std::string> seen_in_work;
      for (const Json& authorship : item["authorships"]) {
        if (!authorship.is_object() || !authorship.contains("author") ||
            !authorship["author"].is_object()) {
          continue;
        }
        const Json& author = authorship["author"];
        const std::string author_id = GetString(author, "id");
        if (!author_id.empty() && selected_authors.contains(author_id) &&
            seen_in_work.insert(author_id).second) {
          work.author_ids.push_back(author_id);
        }
      }
    }

    if (work.author_ids.empty()) {
      continue;
    }

    std::ranges::sort(work.author_ids);
    works.push_back(std::move(work));
  }
  return works;
}

}  // namespace schools::openalex

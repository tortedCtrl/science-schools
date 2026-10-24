#include "openalex/parser.hpp"

#include <algorithm>
#include <boost/json.hpp>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

namespace schools::openalex {
namespace json = boost::json;

namespace {

// "https://openalex.org/A5023888391" -> 5023888391
// "https://openalex.org/fields/17"   -> 17
std::optional<std::int64_t> ParseOpenAlexId(std::string_view url) {
  const auto slash{url.find_last_of('/')};
  std::string_view tail{
      slash == std::string_view::npos ? url : url.substr(slash + 1)};
  while (!tail.empty() &&
         std::isdigit(static_cast<unsigned char>(tail.front())) == 0) {
    tail.remove_prefix(1);
  }
  if (tail.empty()) {
    return std::nullopt;
  }
  std::int64_t value{0};
  const auto [ptr, ec] =
      std::from_chars(tail.data(), tail.data() + tail.size(), value);
  if (ec != std::errc{}) {
    return std::nullopt;
  }
  return value;
}

// Boost.JSON accessors used below (if_contains / if_object / ...) return null
// pointers on mismatch and never throw.
const json::value* Find(const json::object& obj, std::string_view key) {
  const json::value* value{obj.if_contains(key)};
  return (value != nullptr && !value->is_null()) ? value : nullptr;
}

const json::object* GetObject(const json::object& obj, std::string_view key) {
  const json::value* value{Find(obj, key)};
  return value != nullptr ? value->if_object() : nullptr;
}

const json::array* GetArray(const json::object& obj, std::string_view key) {
  const json::value* value{Find(obj, key)};
  return value != nullptr ? value->if_array() : nullptr;
}

std::optional<std::string> GetString(const json::object& obj,
                                     std::string_view key) {
  const json::value* value{Find(obj, key)};
  if (value == nullptr) {
    return std::nullopt;
  }
  const json::string* str{value->if_string()};
  if (str == nullptr) {
    return std::nullopt;
  }
  return std::string(str->begin(), str->end());
}

std::optional<std::int64_t> GetInt(const json::object& obj,
                                   std::string_view key) {
  const json::value* value{Find(obj, key)};
  if (value == nullptr) {
    return std::nullopt;
  }
  if (const auto* number = value->if_int64()) {
    return *number;
  }
  return std::nullopt;
}

// "https://orcid.org/0000-0002-1825-0097" -> "0000-0002-1825-0097"
std::string StripOrcidPrefix(std::string orcid) {
  const auto slash{orcid.find_last_of('/')};
  return slash == std::string::npos ? orcid : orcid.substr(slash + 1);
}

bool HasCountry(const json::object& authorship, std::string_view country_code) {
  if (const json::array* institutions = GetArray(authorship, "institutions");
      institutions != nullptr && !institutions->empty()) {
    return std::ranges::any_of(*institutions, [&](const json::value& inst) {
      const json::object* obj{inst.if_object()};
      if (obj == nullptr) {
        return false;
      }
      return GetString(*obj, "country_code").value_or("") == country_code;
    });
  }
  // Fallback: authorship-level "countries" list.
  if (const json::array* countries = GetArray(authorship, "countries")) {
    return std::ranges::any_of(*countries, [&](const json::value& country) {
      const json::string* str{country.if_string()};
      return str != nullptr && std::string_view(*str) == country_code;
    });
  }
  return false;
}

}  // namespace

Result<domain::WorksPage> ParseWorksPage(std::string_view body,
                                         std::string_view country_code) {
  boost::system::error_code ec;
  const json::value document{json::parse(body, ec)};
  if (ec) {
    return std::unexpected(
        FromBoost(Errc::kParse, "Invalid OpenAlex JSON", ec));
  }
  const json::object* root{document.if_object()};
  if (root == nullptr) {
    return MakeError(Errc::kParse, "OpenAlex response is not a JSON object");
  }
  const json::array* results{GetArray(*root, "results")};
  if (results == nullptr) {
    return MakeError(Errc::kParse, "OpenAlex response has no 'results' array");
  }

  domain::WorksPage page;
  if (const json::object* meta = GetObject(*root, "meta")) {
    page.next_cursor = GetString(*meta, "next_cursor").value_or("");
  }

  std::unordered_set<std::int64_t> seen_authors;
  std::unordered_set<std::int32_t> seen_fields;

  for (const json::value& item : *results) {
    const json::object* work_obj{item.if_object()};
    if (work_obj == nullptr) {
      continue;
    }
    const auto work_id{
        ParseOpenAlexId(GetString(*work_obj, "id").value_or(""))};
    if (!work_id) {
      continue;
    }

    domain::Work work;
    work.openalex_id = *work_id;
    work.publication_year = static_cast<std::int16_t>(
        GetInt(*work_obj, "publication_year").value_or(0));

    if (const json::array* authorships = GetArray(*work_obj, "authorships")) {
      for (const json::value& entry : *authorships) {
        const json::object* authorship{entry.if_object()};
        if (authorship == nullptr || !HasCountry(*authorship, country_code)) {
          continue;
        }
        const json::object* author_obj{GetObject(*authorship, "author")};
        if (author_obj == nullptr) {
          continue;
        }
        const auto author_id{
            ParseOpenAlexId(GetString(*author_obj, "id").value_or(""))};
        if (!author_id) {
          continue;
        }

        work.ru_author_ids.push_back(*author_id);
        if (seen_authors.insert(*author_id).second) {
          domain::Author author;
          author.openalex_id = *author_id;
          author.name = GetString(*author_obj, "display_name").value_or("");
          if (auto orcid = GetString(*author_obj, "orcid")) {
            author.orcid = StripOrcidPrefix(std::move(*orcid));
          }
          page.authors.push_back(std::move(author));
        }
      }
    }
    if (work.ru_author_ids.empty()) {
      continue;
    }
    std::ranges::sort(work.ru_author_ids);
    const auto duplicates{std::ranges::unique(work.ru_author_ids)};
    work.ru_author_ids.erase(duplicates.begin(), duplicates.end());

    if (const json::array* topics = GetArray(*work_obj, "topics")) {
      for (const json::value& entry : *topics) {
        const json::object* topic{entry.if_object()};
        if (topic == nullptr) {
          continue;
        }
        const json::object* field_obj{GetObject(*topic, "field")};
        if (field_obj == nullptr) {
          continue;
        }
        const auto field_id{
            ParseOpenAlexId(GetString(*field_obj, "id").value_or(""))};
        if (!field_id) {
          continue;
        }
        const auto id{static_cast<std::int32_t>(*field_id)};
        if (std::ranges::find(work.field_ids, id) == work.field_ids.end()) {
          work.field_ids.push_back(id);
        }
        if (seen_fields.insert(id).second) {
          page.fields.push_back(
              {id, GetString(*field_obj, "display_name").value_or("")});
        }
      }
    }
    page.works.push_back(std::move(work));
  }
  return page;
}

}  // namespace schools::openalex

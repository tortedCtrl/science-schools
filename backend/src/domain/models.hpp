#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schools::domain {

// OpenAlex identifiers are kept in their canonical URL form
// ("https://openalex.org/A123"), because that is what the API returns and what
// the existing database schema stores.
struct Author {
  std::string openalex_id;
  std::string name;
  std::string institution_name;
};

struct Work {
  std::string openalex_id;
  std::string title;
  std::int32_t year{0};
  std::vector<std::string> author_ids;
};

struct Edge {
  std::int32_t source{0};
  std::int32_t target{0};
  std::int32_t weight{0};
};

struct Article {
  std::string openalex_id;
  std::string title;
  std::int32_t year{0};
};

struct AuthorArticle {
  std::int32_t author_id{0};
  std::string article_id;
};

struct AuthorSummary {
  std::int32_t id{0};
  std::string openalex_id;
  std::string name;
  std::optional<std::int32_t> community_id;
  std::optional<std::string> institution_name;
  std::int64_t weighted_degree{0};
};

struct AuthorCard {
  AuthorSummary author;
  std::vector<Article> articles;
};

struct GraphData {
  std::vector<AuthorSummary> nodes;
  std::vector<Edge> links;
};

// A Leiden clustering result. Membership is deliberately stored apart from the
// author row so the author table only contains author data.
struct Membership {
  std::int32_t author_id{0};
  std::int32_t community_id{0};
};

struct IngestResult {
  std::size_t authors{0};
  std::size_t works{0};
  std::size_t edges{0};
  std::size_t articles{0};
  std::size_t communities{0};
};

}  // namespace schools::domain

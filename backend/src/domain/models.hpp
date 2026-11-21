#pragma once

// #include <cstddef>
// #include <cstdint>
// #include <optional>
// #include <string>
// #include <vector>

// namespace schools::domain {

// // OpenAlex identifiers are kept in their canonical URL form
// // ("https://openalex.org/A123"), because that is what the API returns and what
// // the existing database schema stores.
// struct Author {
//   std::string openalex_id;
//   std::string name;
//   std::string institution_name;
// };


// struct ArticleTopic {
//     std::string topic_id;
//     std::string name;
//     double score{0.0};

//     std::string subfield_id;
//     std::string subfield_name;

//     std::string field_id;
//     std::string field_name;

//     std::string domain_id;
//     std::string domain_name;

//     bool is_primary{false};
// };

// struct Work {
//   std::string openalex_id;
//   std::string title;
//   std::int32_t year{0};
//   std::vector<std::string> author_ids;
//   std::vector<ArticleTopic> topics;
// };

// struct Edge {
//   std::int32_t source{0};
//   std::int32_t target{0};
//   std::int32_t weight{0};
// };

// struct Article {
//   std::string openalex_id;
//   std::string title;
//   std::int32_t year{0};
//   std::vector<ArticleTopic> topics;
// };

// struct ArticleTopicInput {
//   Article article;
//   std::vector<std::int32_t> author_ids;
// };

// struct AuthorArticle {
//   std::int32_t author_id{0};
//   std::string article_id;
// };

// struct AuthorSummary {
//   std::int32_t id{0};
//   std::string openalex_id;
//   std::string name;
//   std::optional<std::int32_t> community_id;
//   std::optional<std::string> institution_name;
//   std::int64_t weighted_degree{0};
// };

// struct AuthorCard {
//   AuthorSummary author;
//   std::vector<Article> articles;
// };

// struct GraphData {
//   std::vector<AuthorSummary> nodes;
//   std::vector<Edge> links;
// };

// // A Leiden clustering result. Membership is deliberately stored apart from the
// // author row so the author table only contains author data.
// struct Membership {
//   std::int64_t run_id{0};
//   std::int32_t author_id{0};
//   std::int32_t community_id{0};
// };

// struct CommunityTopic {
//   std::int64_t run_id{0};
//   std::string algorithm;
//   std::int32_t community_id{0};
//   std::string topic_id;
//   std::string topic_name;
//   double weight{0.0};
//   std::int32_t article_count{0};
// };

// enum class CommunityAlgorithm { kLeiden, kBigClam };

// struct IngestResult {
//   std::size_t authors{0};
//   std::size_t works{0};
//   std::size_t edges{0};
//   std::size_t articles{0};
//   std::size_t communities{0};
// };

// }  // namespace schools::domain



#pragma once

// Domain DB Entities (map 1:1 to database tables)
#include "domain/db/author.hpp"
#include "domain/db/edge.hpp"
#include "domain/db/article.hpp"
#include "domain/db/author_article.hpp"
#include "domain/db/article_topic.hpp"
#include "domain/db/membership.hpp"
#include "domain/db/community_topic.hpp"
#include "domain/db/clustering_run.hpp"

// DTO / API Models (sent to frontend)
#include "domain/dto/author_summary.hpp"
#include "domain/dto/author_card.hpp"
#include "domain/dto/graph_data.hpp"
#include "domain/dto/community_summary.hpp"
#include "domain/dto/api_response.hpp"

// Internal / Processing Models
#include "domain/internal/work.hpp"
#include "domain/internal/article_topic_input.hpp"
#include "domain/internal/ingest_result.hpp"

// Common Types & Enums
#include "domain/common/types.hpp"
#include "domain/common/enums.hpp"
#include "domain/common/topic_hierarchy.hpp"
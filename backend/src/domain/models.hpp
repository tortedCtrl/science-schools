#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace schools::domain {

// ---- Ingestion models (OpenAlex -> database) ------------------------------

struct Author {
  std::int64_t openalex_id{0};  // numeric part of "A5023888391"
  std::optional<std::string> orcid;
  std::string name;
};

struct Field {
  std::int32_t id{0};  // OpenAlex field id, e.g. 17 = Computer Science
  std::string name;
};

struct Work {
  std::int64_t openalex_id{0};  // numeric part of "W2741809807"
  std::int16_t publication_year{0};
  std::vector<std::int64_t> ru_author_ids;  // only authors with RU affiliation
  std::vector<std::int32_t> field_ids;
};

// One page of the OpenAlex /works response, already filtered and normalized.
struct WorksPage {
  std::vector<Author> authors;
  std::vector<Field> fields;
  std::vector<Work> works;
  std::string next_cursor;  // empty when this was the last page
};

// ---- Graph / clustering models --------------------------------------------

struct WeightedEdge {
  std::int64_t src{0};
  std::int64_t dst{0};
  float weight{0.0F};
};

struct MembershipRow {
  std::int64_t author_id{0};
  std::int32_t community_id{0};
  float weighted_degree{0.0F};
};

struct RunParams {
  std::string algorithm;
  double resolution{1.0};
  std::int64_t vertices{0};
  std::int64_t edges{0};
};

// ---- Query models (API responses) -----------------------------------------

struct FieldCount {
  std::int32_t id{0};
  std::string name;
  std::int32_t works_count{0};
};

struct AuthorMembership {
  std::int32_t community_id{0};
  std::int32_t size{0};
};

struct AuthorCommunity {
  Author author;
  std::int64_t run_id{0};
  std::int32_t community_id{0};
  std::int32_t community_size{0};
  std::vector<FieldCount> author_fields;
  std::vector<FieldCount> community_top_fields;
};

struct CommunityMember {
  std::int64_t author_id{0};
  std::string name;
  std::optional<std::string> orcid;
  float weighted_degree{0.0F};
};

struct CommunitySummary {
  std::int32_t community_id{0};
  std::int32_t size{0};
};

struct CommunityDetails {
  std::int64_t run_id{0};
  std::int32_t community_id{0};
  std::int32_t size{0};
  std::vector<FieldCount> top_fields;
  std::vector<CommunityMember> members;
};

struct GraphNode {
  std::int64_t author_id{0};
  std::string name;
  std::int32_t community_id{0};
  float weighted_degree{0.0F};
};

struct GraphSlice {
  std::int64_t run_id{0};
  std::vector<GraphNode> nodes;
  std::vector<WeightedEdge> edges;
  bool truncated{false};  // more nodes exist than the limit allowed
};

}  // namespace schools::domain

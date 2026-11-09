#include "repository/community_repository.hpp"

#include <soci/soci.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <string>
#include <vector>

#include "repository/bulk_insert.hpp"

namespace schools::repository {
namespace {

void InsertCommunityTopics(
    soci::session& sql, std::span<const domain::CommunityTopic> topics) {
  std::vector<std::string> algorithms;
  std::vector<std::int32_t> community_ids;
  std::vector<std::string> topic_ids;
  std::vector<std::string> topic_names;
  std::vector<double> weights;
  std::vector<std::int32_t> article_counts;
  algorithms.reserve(topics.size());
  community_ids.reserve(topics.size());
  topic_ids.reserve(topics.size());
  topic_names.reserve(topics.size());
  weights.reserve(topics.size());
  article_counts.reserve(topics.size());
  for (const domain::CommunityTopic& topic : topics) {
    algorithms.push_back(topic.algorithm);
    community_ids.push_back(topic.community_id);
    topic_ids.push_back(topic.topic_id);
    topic_names.push_back(topic.topic_name);
    weights.push_back(topic.weight);
    article_counts.push_back(topic.article_count);
  }

  for (std::size_t offset = 0; offset < community_ids.size();
       offset += kBulkChunkSize) {
    const std::size_t end =
        std::min(offset + kBulkChunkSize, community_ids.size());
    auto algorithm_chunk = Slice(algorithms, offset, end);
    auto community_chunk = Slice(community_ids, offset, end);
    auto topic_id_chunk = Slice(topic_ids, offset, end);
    auto topic_name_chunk = Slice(topic_names, offset, end);
    auto weight_chunk = Slice(weights, offset, end);
    auto article_count_chunk = Slice(article_counts, offset, end);
    soci::statement statement =
        (sql.prepare << "INSERT INTO community_topics "
                        "(algorithm, community_id, topic_id, topic_name, "
                        "weight, article_count) "
                        "VALUES (:algorithm, :community_id, :topic_id, "
                        ":topic_name, :weight, :article_count)",
         soci::use(algorithm_chunk), soci::use(community_chunk),
         soci::use(topic_id_chunk), soci::use(topic_name_chunk),
         soci::use(weight_chunk), soci::use(article_count_chunk));
    statement.execute(true);
  }
}

void InsertMemberships(
    soci::session& sql, std::span<const domain::Membership> memberships,
    const char* table_name, bool update_conflicts) {
  std::vector<std::int32_t> author_ids;
  std::vector<std::int32_t> community_ids;
  author_ids.reserve(memberships.size());
  community_ids.reserve(memberships.size());
  for (const domain::Membership& membership : memberships) {
    author_ids.push_back(membership.author_id);
    community_ids.push_back(membership.community_id);
  }

  for (std::size_t offset = 0; offset < author_ids.size();
       offset += kBulkChunkSize) {
    const std::size_t end =
        std::min(offset + kBulkChunkSize, author_ids.size());
    auto author_chunk = Slice(author_ids, offset, end);
    auto community_chunk = Slice(community_ids, offset, end);
    const std::string query =
        std::string("INSERT INTO ") + table_name +
        " (author_id, community_id) VALUES (:author_id, :community_id) " +
        (update_conflicts
             ? "ON CONFLICT (author_id) DO UPDATE "
               "SET community_id = EXCLUDED.community_id"
             : "ON CONFLICT DO NOTHING");
    soci::statement statement =
        (sql.prepare << query, soci::use(author_chunk),
         soci::use(community_chunk));
    statement.execute(true);
  }
}

}  // namespace

Status CommunityRepository::ReplaceLeidenResults(
    std::span<const domain::Membership> memberships,
    std::span<const domain::CommunityTopic> topics) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
    sql << "DELETE FROM community_topics WHERE algorithm = 'leiden'";
    sql << "DELETE FROM author_communities";
    InsertMemberships(sql, memberships, "author_communities", true);
    InsertCommunityTopics(sql, topics);
    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "save Leiden communities and topics: {}",
                     error.what());
  }
}

Result<std::vector<domain::CommunityTopic>>
CommunityRepository::ListTopCommunityTopics(std::string_view algorithm) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const std::string algorithm_name{algorithm};
    const soci::rowset<soci::row> rows =
        (sql.prepare << "SELECT community_id, topic_id, topic_name, weight, "
                        "article_count FROM ("
                        "  SELECT community_id, topic_id, topic_name, weight, "
                        "         article_count, "
                        "         ROW_NUMBER() OVER ("
                        "           PARTITION BY community_id "
                        "           ORDER BY weight DESC, topic_id"
                        "         ) AS topic_rank "
                        "  FROM community_topics WHERE algorithm = :algorithm"
                        ") ranked "
                        "WHERE topic_rank <= 5 "
                        "ORDER BY community_id, topic_rank",
         soci::use(algorithm_name));
    std::vector<domain::CommunityTopic> topics;
    for (const soci::row& row : rows) {
      topics.push_back({.algorithm = algorithm_name,
                        .community_id = row.get<std::int32_t>(0),
                        .topic_id = row.get<std::string>(1),
                        .topic_name = row.get<std::string>(2),
                        .weight = row.get<double>(3),
                        .article_count = row.get<std::int32_t>(4)});
    }
    return topics;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list top community topics: {}",
                     error.what());
  }
}

Result<std::vector<domain::Membership>>
CommunityRepository::ListBigClamMemberships() {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }
  soci::session& sql = **session;
  try {
    const soci::rowset<soci::row> rows =
        (sql.prepare << "SELECT author_id, community_id "
                        "FROM bigclam_memberships "
                        "ORDER BY author_id, community_id");
    std::vector<domain::Membership> memberships;
    for (const soci::row& row : rows) {
      memberships.push_back({.author_id = row.get<std::int32_t>(0),
                             .community_id = row.get<std::int32_t>(1)});
    }
    return memberships;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list BigCLAM memberships: {}",
                     error.what());
  }
}

Status CommunityRepository::ReplaceBigClamResults(
    std::span<const domain::Membership> memberships,
    std::span<const domain::CommunityTopic> topics) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
    sql << "DELETE FROM bigclam_memberships";
    sql << "DELETE FROM community_topics WHERE algorithm = 'bigclam'";
    InsertMemberships(sql, memberships, "bigclam_memberships", false);
    InsertCommunityTopics(sql, topics);
    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "save BigCLAM communities and topics: {}",
                     error.what());
  }
}

}  // namespace schools::repository

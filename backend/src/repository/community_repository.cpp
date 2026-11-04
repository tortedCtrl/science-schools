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
    sql << "DELETE FROM community_topics";
    sql << "DELETE FROM author_communities";

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
      soci::statement statement =
          (sql.prepare << "INSERT INTO author_communities "
                          "(author_id, community_id) "
                          "VALUES (:author_id, :community_id) "
                          "ON CONFLICT (author_id) DO UPDATE "
                          "SET community_id = EXCLUDED.community_id",
           soci::use(author_chunk), soci::use(community_chunk));
      statement.execute(true);
    }

    std::vector<std::int32_t> topic_community_ids;
    std::vector<std::string> topic_ids;
    std::vector<std::string> topic_names;
    std::vector<double> topic_weights;
    std::vector<std::int32_t> article_counts;
    topic_community_ids.reserve(topics.size());
    topic_ids.reserve(topics.size());
    topic_names.reserve(topics.size());
    topic_weights.reserve(topics.size());
    article_counts.reserve(topics.size());
    for (const domain::CommunityTopic& topic : topics) {
      topic_community_ids.push_back(topic.community_id);
      topic_ids.push_back(topic.topic_id);
      topic_names.push_back(topic.topic_name);
      topic_weights.push_back(topic.weight);
      article_counts.push_back(topic.article_count);
    }

    for (std::size_t offset = 0; offset < topic_community_ids.size();
         offset += kBulkChunkSize) {
      const std::size_t end =
          std::min(offset + kBulkChunkSize, topic_community_ids.size());
      auto community_chunk = Slice(topic_community_ids, offset, end);
      auto topic_id_chunk = Slice(topic_ids, offset, end);
      auto topic_name_chunk = Slice(topic_names, offset, end);
      auto weight_chunk = Slice(topic_weights, offset, end);
      auto article_count_chunk = Slice(article_counts, offset, end);
      soci::statement statement =
          (sql.prepare << "INSERT INTO community_topics "
                          "(community_id, topic_id, topic_name, weight, "
                          "article_count) "
                          "VALUES (:community_id, :topic_id, :topic_name, "
                          ":weight, :article_count)",
           soci::use(community_chunk), soci::use(topic_id_chunk),
           soci::use(topic_name_chunk), soci::use(weight_chunk),
           soci::use(article_count_chunk));
      statement.execute(true);
    }

    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "save Leiden communities and topics: {}",
                     error.what());
  }
}

Result<std::vector<domain::CommunityTopic>>
CommunityRepository::ListTopCommunityTopics() {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const soci::rowset<soci::row> rows =
        (sql.prepare << "SELECT community_id, topic_id, topic_name, weight, "
                        "article_count FROM ("
                        "  SELECT community_id, topic_id, topic_name, weight, "
                        "         article_count, "
                        "         ROW_NUMBER() OVER ("
                        "           PARTITION BY community_id "
                        "           ORDER BY weight DESC, topic_id"
                        "         ) AS topic_rank "
                        "  FROM community_topics"
                        ") ranked "
                        "WHERE topic_rank <= 5 "
                        "ORDER BY community_id, topic_rank");
    std::vector<domain::CommunityTopic> topics;
    for (const soci::row& row : rows) {
      topics.push_back({.community_id = row.get<std::int32_t>(0),
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

  Result<std::vector<domain::Membership>> CommunityRepository::ListBigClamMemberships() {
    auto session = database_.Acquire();
    if (!session) {
      return std::unexpected(session.error());
    }
    soci::session& sql = **session;
    try {
      const soci::rowset<soci::row> rows = (sql.prepare<<"SELECT author_id, community_id "
                        "FROM bigclam_memberships "
                        "ORDER BY author_id, community_id");
      std::vector<domain::Membership> memberships;
      for (const soci::row& row: rows) {
        memberships.push_back({.author_id = row.get<std::int32_t>(0),.community_id = row.get<std::int32_t>(1)});
      }
      return memberships;
    } catch (const std::exception& error){
      return MakeError(Errc::kDatabase, "list BigCLAM memberships: {}", error.what());
    }
}
Status CommunityRepository::ReplaceBigClamMemberships(std::span<const domain::Membership> memberships){
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
    sql << "DELETE FROM bigclam_memberships";

    std::vector<std::int32_t> author_ids;
    std::vector<std::int32_t> community_ids;
    author_ids.reserve(memberships.size());
    community_ids.reserve(memberships.size());
    for (const auto& membership : memberships) {
      author_ids.push_back(membership.author_id);
      community_ids.push_back(membership.community_id);
    }

    for (std::size_t offset = 0; offset < author_ids.size();
         offset += kBulkChunkSize) {
      const std::size_t end =
          std::min(offset + kBulkChunkSize, author_ids.size());
      auto author_chunk = Slice(author_ids, offset, end);
      auto community_chunk = Slice(community_ids, offset, end);
      soci::statement statement =
          (sql.prepare << "INSERT INTO bigclam_memberships "
                          "(author_id, community_id) "
                          "VALUES (:author_id, :community_id) "
                          "ON CONFLICT DO NOTHING",
           soci::use(author_chunk), soci::use(community_chunk));
      statement.execute(true);
    }

    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "save BigCLAM communities: {}",
                     error.what());
  }
}
}  // namespace schools::repository

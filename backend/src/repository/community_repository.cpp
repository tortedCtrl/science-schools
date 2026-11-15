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
    soci::session& sql, std::span<const domain::CommunityTopic> topics,
    std::int64_t run_id) {
  std::vector<std::int64_t> run_ids;
  std::vector<std::int32_t> community_ids;
  std::vector<std::string> topic_ids;
  std::vector<std::string> topic_names;
  std::vector<double> weights;
  std::vector<std::int32_t> article_counts;
  run_ids.reserve(topics.size());
  community_ids.reserve(topics.size());
  topic_ids.reserve(topics.size());
  topic_names.reserve(topics.size());
  weights.reserve(topics.size());
  article_counts.reserve(topics.size());
  for (const domain::CommunityTopic& topic : topics) {
    run_ids.push_back(run_id);
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
    auto run_id_chunk = Slice(run_ids, offset, end);
    auto community_chunk = Slice(community_ids, offset, end);
    auto topic_id_chunk = Slice(topic_ids, offset, end);
    auto topic_name_chunk = Slice(topic_names, offset, end);
    auto weight_chunk = Slice(weights, offset, end);
    auto article_count_chunk = Slice(article_counts, offset, end);
    soci::statement statement =
        (sql.prepare << "INSERT INTO community_topics "
                        "(run_id, community_id, topic_id, topic_name, "
                        "weight, article_count) "
                        "VALUES (:run_id, :community_id, :topic_id, "
                        ":topic_name, :weight, :article_count)",
         soci::use(run_id_chunk), soci::use(community_chunk),
         soci::use(topic_id_chunk), soci::use(topic_name_chunk),
         soci::use(weight_chunk), soci::use(article_count_chunk));
    statement.execute(true);
  }
}

void InsertMemberships(
    soci::session& sql, std::span<const domain::Membership> memberships,
    std::int64_t run_id) {
  std::vector<std::int64_t> run_ids;
  std::vector<std::int32_t> author_ids;
  std::vector<std::int32_t> community_ids;
  run_ids.reserve(memberships.size());
  author_ids.reserve(memberships.size());
  community_ids.reserve(memberships.size());
  for (const domain::Membership& membership : memberships) {
    run_ids.push_back(run_id);
    author_ids.push_back(membership.author_id);
    community_ids.push_back(membership.community_id);
  }

  for (std::size_t offset = 0; offset < author_ids.size();
       offset += kBulkChunkSize) {
    const std::size_t end =
        std::min(offset + kBulkChunkSize, author_ids.size());
    auto run_id_chunk = Slice(run_ids, offset, end);
    auto author_chunk = Slice(author_ids, offset, end);
    auto community_chunk = Slice(community_ids, offset, end);
    soci::statement statement =
        (sql.prepare << "INSERT INTO author_communities "
                        "(run_id, author_id, community_id) "
                        "VALUES (:run_id, :author_id, :community_id) "
                        "ON CONFLICT DO NOTHING",
         soci::use(run_id_chunk), soci::use(author_chunk),
         soci::use(community_chunk));
    statement.execute(true);
  }
}

}  // namespace

Result<std::int64_t> CommunityRepository::SaveLeidenResults(
    std::span<const domain::Membership> memberships,
    std::span<const domain::CommunityTopic> topics) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
    std::int64_t run_id{0};
    sql << "INSERT INTO clustering_runs (algorithm) VALUES ('leiden') "
           "RETURNING id",
        soci::into(run_id);
    InsertMemberships(sql, memberships, run_id);
    InsertCommunityTopics(sql, topics, run_id);
    transaction.commit();
    return run_id;
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
        (sql.prepare << "WITH latest_run AS ("
                        "  SELECT id FROM clustering_runs "
                        "  WHERE algorithm = :algorithm "
                        "  ORDER BY id DESC LIMIT 1"
                        "), ranked AS ("
                        "  SELECT ct.run_id, ct.community_id, ct.topic_id, "
                        "         ct.topic_name, ct.weight, ct.article_count, "
                        "         ROW_NUMBER() OVER ("
                        "           PARTITION BY ct.community_id "
                        "           ORDER BY ct.weight DESC, ct.topic_id"
                        "         ) AS topic_rank "
                        "  FROM community_topics ct "
                        "  JOIN latest_run lr ON lr.id = ct.run_id"
                        ") "
                        "SELECT run_id, community_id, topic_id, topic_name, "
                        "       weight, article_count "
                        "FROM ranked WHERE topic_rank <= 5 "
                        "ORDER BY community_id, topic_rank",
         soci::use(algorithm_name));
    std::vector<domain::CommunityTopic> topics;
    for (const soci::row& row : rows) {
      topics.push_back({.run_id = row.get<std::int64_t>(0),
                        .algorithm = algorithm_name,
                        .community_id = row.get<std::int32_t>(1),
                        .topic_id = row.get<std::string>(2),
                        .topic_name = row.get<std::string>(3),
                        .weight = row.get<double>(4),
                        .article_count = row.get<std::int32_t>(5)});
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
        (sql.prepare << "SELECT run_id, author_id, community_id "
                        "FROM author_communities "
                        "WHERE run_id = (SELECT MAX(id) FROM clustering_runs "
                        "                WHERE algorithm = 'bigclam') "
                        "ORDER BY author_id, community_id");
    std::vector<domain::Membership> memberships;
    for (const soci::row& row : rows) {
      memberships.push_back({.run_id = row.get<std::int64_t>(0),
                             .author_id = row.get<std::int32_t>(1),
                             .community_id = row.get<std::int32_t>(2)});
    }
    return memberships;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list BigCLAM memberships: {}",
                     error.what());
  }
}

Result<std::int64_t> CommunityRepository::SaveBigClamResults(
    std::span<const domain::Membership> memberships,
    std::span<const domain::CommunityTopic> topics) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
    std::int64_t run_id{0};
    sql << "INSERT INTO clustering_runs (algorithm) VALUES ('bigclam') "
           "RETURNING id",
        soci::into(run_id);
    InsertMemberships(sql, memberships, run_id);
    InsertCommunityTopics(sql, topics, run_id);
    transaction.commit();
    return run_id;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "save BigCLAM communities and topics: {}",
                     error.what());
  }
}

}  // namespace schools::repository

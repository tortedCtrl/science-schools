#include "repository/community_repository.hpp"

#include <soci/soci.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <string>
#include <vector>
#include <map>
#include <utility>

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

Result<std::vector<domain::CommunitySummary>>
CommunityRepository::ListCommunities(std::string_view algorithm) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const std::string algorithm_name{algorithm};
    const soci::rowset<soci::row> rows =
        (sql.prepare << "SELECT ac.community_id, "
                        "COUNT(DISTINCT ac.author_id) "
                        "FROM author_communities ac "
                        "WHERE ac.run_id = (SELECT id FROM clustering_runs "
                        "WHERE algorithm = :algorithm "
                        "ORDER BY id DESC LIMIT 1) "
                        "GROUP BY ac.community_id ORDER BY ac.community_id",
         soci::use(algorithm_name));
    std::vector<domain::CommunitySummary> communities;
    for (const soci::row& row : rows) {
      communities.push_back(
          {.community_id = row.get<std::int32_t>(0),
           .member_count = row.get<std::int64_t>(1)});
    }
    return communities;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list communities: {}",
                     error.what());
  }
}

Result<std::vector<domain::CommunityTopic>>
CommunityRepository::ListCommunityTopics(std::string_view algorithm,
                                         std::int32_t community_id) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const std::string algorithm_name{algorithm};
    const soci::rowset<soci::row> rows =
        (sql.prepare << "SELECT ct.run_id, ct.community_id, ct.topic_id, "
                        "ct.topic_name, ct.weight, ct.article_count "
                        "FROM community_topics ct "
                        "WHERE ct.community_id = :community_id "
                        "AND ct.run_id = (SELECT id FROM clustering_runs "
                        "WHERE algorithm = :algorithm "
                        "ORDER BY id DESC LIMIT 1) "
                        "ORDER BY ct.weight DESC, ct.topic_id LIMIT 5",
         soci::use(community_id), soci::use(algorithm_name));
    const auto algorithm_value =
        algorithm_name == "bigclam"
            ? domain::CommunityAlgorithm::kBigClam
            : domain::CommunityAlgorithm::kLeiden;
    std::vector<domain::CommunityTopic> topics;
    for (const soci::row& row : rows) {
      topics.push_back({.run_id = row.get<std::int64_t>(0),
                        .algorithm = algorithm_value,
                        .community_id = row.get<std::int32_t>(1),
                        .topic_id = row.get<std::string>(2),
                        .topic_name = row.get<std::string>(3),
                        .weight = row.get<double>(4),
                        .article_count = row.get<std::int32_t>(5)});
    }
    return topics;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list community topics: {}",
                     error.what());
  }
}


Result<std::vector<domain::TopicInfo>>
CommunityRepository::ListAllTopics() {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const soci::rowset<soci::row> rows =
        (sql.prepare << "SELECT topic_id, topic_name, "
                        "COUNT(DISTINCT article_id) "
                        "FROM article_topics "
                        "GROUP BY topic_id, topic_name "
                        "ORDER BY topic_name, topic_id");

    std::vector<domain::TopicInfo> topics;
    for (const soci::row& row : rows) {
      topics.push_back({
          .topic_id = row.get<std::string>(0),
          .topic_name = row.get<std::string>(1),
          .article_count = row.get<std::int64_t>(2),
      });
    }
    return topics;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list all topics: {}",
                     error.what());
  }
}

Result<std::vector<domain::ClusterMetadata>>
CommunityRepository::ListClusterMetadata(std::string_view algorithm) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const std::string algorithm_name{algorithm};

    const soci::rowset<soci::row> rows =
      (sql.prepare << "SELECT ac.community_id, "
                      "COUNT(DISTINCT ac.author_id) "
                      "FROM clustering_runs cr "
                      "JOIN author_communities ac ON ac.run_id = cr.id "
                      "WHERE cr.id = ("
                      "SELECT id FROM clustering_runs "
                      "WHERE algorithm = :algorithm "
                      "ORDER BY id DESC LIMIT 1) "
                      "GROUP BY ac.community_id "
                      "ORDER BY ac.community_id",
      soci::use(algorithm_name));

    std::vector<domain::ClusterMetadata> clusters;
    for (const soci::row& row : rows) {
      clusters.push_back({
          .community_id = row.get<std::int32_t>(0),
          .member_count = row.get<std::int32_t>(1),
      });
    }
    return clusters;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list cluster metadata: {}",
                     error.what());
  }
}

Result<std::vector<domain::ClusterByTopic>>
CommunityRepository::ListClustersByTopic(
    std::string_view algorithm, std::string_view topic_id) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    const std::string algorithm_name{algorithm};
    const std::string topic_id_value{topic_id};

    const soci::rowset<soci::row> rows =
        (sql.prepare
         << "WITH latest_run AS ("
            "SELECT id FROM clustering_runs "
            "WHERE algorithm = :algorithm "
            "ORDER BY id DESC LIMIT 1"
            "), target_communities AS ("
            "SELECT DISTINCT ct.community_id "
            "FROM community_topics ct "
            "JOIN latest_run lr ON lr.id = ct.run_id "
            "WHERE ct.topic_id = :topic_id"
            "), community_counts AS ("
            "SELECT ac.community_id, "
            "COUNT(DISTINCT ac.author_id) AS member_count "
            "FROM author_communities ac "
            "JOIN latest_run lr ON lr.id = ac.run_id "
            "JOIN target_communities tc "
            "ON tc.community_id = ac.community_id "
            "GROUP BY ac.community_id"
            ") "
            "SELECT cc.community_id, cc.member_count, "
            "a.id, a.openalex_id, a.name "
            "FROM community_counts cc "
            "JOIN author_communities ac "
            "ON ac.community_id = cc.community_id "
            "JOIN latest_run lr ON lr.id = ac.run_id "
            "JOIN authors a ON a.id = ac.author_id "
            "ORDER BY cc.community_id, a.id",
         soci::use(algorithm_name), soci::use(topic_id_value));

    std::map<std::int32_t, domain::ClusterByTopic> cluster_map;

    for (const soci::row& row : rows) {
      const auto community_id = row.get<std::int32_t>(0);

      auto result = cluster_map.try_emplace(
          community_id,
          domain::ClusterByTopic{
              .community_id = community_id,
              .member_count = row.get<std::int32_t>(1),
          });

      result.first->second.authors.push_back({
          .author_id = row.get<std::int32_t>(2),
          .openalex_id = row.get<std::string>(3),
          .name = row.get<std::string>(4),
      });
    }

    std::vector<domain::ClusterByTopic> clusters;
    clusters.reserve(cluster_map.size());

    for (auto& entry : cluster_map) {
      clusters.push_back(std::move(entry.second));
    }

    return clusters;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "list clusters by topic: {}",
                     error.what());
  }
}


Result<domain::ClusterContent> CommunityRepository::GetClusterContent(
    std::string_view algorithm, std::int32_t community_id) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;

  try {
    const std::string algorithm_name{algorithm};

    domain::ClusterContent content;
    content.community_id = community_id;

    // Проверяем существование кластера в последнем запуске алгоритма.
    const soci::rowset<soci::row> metadata_rows =
        (sql.prepare
         << "WITH latest_run AS ("
            "SELECT id FROM clustering_runs "
            "WHERE algorithm = :algorithm "
            "ORDER BY id DESC LIMIT 1"
            ") "
            "SELECT lr.id, COUNT(DISTINCT ac.author_id) "
            "FROM latest_run lr "
            "JOIN author_communities ac ON ac.run_id = lr.id "
            "WHERE ac.community_id = :community_id "
            "GROUP BY lr.id",
         soci::use(algorithm_name), soci::use(community_id));

    auto metadata_it = metadata_rows.begin();
    if (metadata_it == metadata_rows.end()) {
      return MakeError(Errc::kNotFound,
                       "Cluster {} not found for algorithm '{}'",
                       community_id, algorithm_name);
    }

    const auto run_id = metadata_it->get<std::int64_t>(0);
    content.member_count = metadata_it->get<std::int32_t>(1);

    // Получаем авторов кластера.
    const soci::rowset<soci::row> author_rows =
        (sql.prepare
         << "SELECT a.id, a.openalex_id, a.name "
            "FROM author_communities ac "
            "JOIN authors a ON a.id = ac.author_id "
            "WHERE ac.run_id = :run_id "
            "AND ac.community_id = :community_id "
            "ORDER BY a.name",
          soci::use(run_id), soci::use(community_id));

    for (const soci::row& row : author_rows) {
      content.authors.push_back({
          .author_id = row.get<std::int32_t>(0),
          .openalex_id = row.get<std::string>(1),
          .name = row.get<std::string>(2),
      });
    }

    // Получаем темы кластера.
    const soci::rowset<soci::row> topic_rows =
        (sql.prepare
         << "SELECT topic_id, topic_name, article_count "
            "FROM community_topics "
            "WHERE run_id = :run_id "
            "AND community_id = :community_id "
            "ORDER BY article_count DESC, topic_name",
         soci::use(run_id), soci::use(community_id));

    for (const soci::row& row : topic_rows) {
      content.topics.push_back({
          .topic_id = row.get<std::string>(0),
          .topic_name = row.get<std::string>(1),
          .article_count = row.get<std::int64_t>(2),
      });
    }

    return content;
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase,
                     "get cluster content: {}", error.what());
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

#include "repository/community_repository.hpp"

#include <soci/soci.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <vector>

#include "repository/bulk_insert.hpp"

namespace schools::repository {

Status CommunityRepository::ReplaceMemberships(
    std::span<const domain::Membership> memberships) {
  auto session = database_.Acquire();
  if (!session) {
    return std::unexpected(session.error());
  }

  soci::session& sql = **session;
  try {
    soci::transaction transaction(sql);
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

    transaction.commit();
    return {};
  } catch (const std::exception& error) {
    return MakeError(Errc::kDatabase, "save author communities: {}",
                     error.what());
  }
}

}  // namespace schools::repository

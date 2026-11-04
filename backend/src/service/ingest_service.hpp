#pragma once

#include <span>
#include <vector>

#include "common/config.hpp"
#include "common/error.hpp"
#include "common/rate_limiter.hpp"
#include "domain/models.hpp"
#include "openalex/client.hpp"
#include "repository/community_repository.hpp"
#include "repository/ingest_repository.hpp"

namespace schools::service {

// OpenAlex -> domain graph -> PostgreSQL. Network requests are parallelised
// across independent author/work batches; the final write is one transaction
// with multi-row INSERTs.
class IngestService {
 public:
  IngestService(const Config& config, repository::IngestRepository& repository,
                repository::CommunityRepository& communities);

  Result<domain::IngestResult> Run();

 private:
  Result<std::vector<domain::Author>> LoadAuthors();
  Result<std::vector<domain::Work>> LoadWorks(
      std::span<const domain::Author> authors);

  [[nodiscard]] openalex::ClientOptions ClientOptions() const;

  const Config& config_;
  repository::IngestRepository& repository_;
  repository::CommunityRepository& communities_;
  RateLimiter limiter_;
};

}  // namespace schools::service

#pragma once

#include <span>
#include <vector>

#include "common/config.hpp"
#include "common/error.hpp"
#include "common/rate_limiter.hpp"
#include "domain/models.hpp"
#include "openalex/client.hpp"
#include "repository/community_repository.hpp"
#include "repository/graph_repository.hpp"
#include "repository/ingest_repository.hpp"

namespace schools::service {

// OpenAlex -> domain graph -> PostgreSQL. Network requests are parallelised
// across independent author/work batches; the final write is one transaction
// with multi-row INSERTs.
class IngestService {
 public:
  IngestService(const Config& config, repository::IngestRepository& repository,
                repository::CommunityRepository& communities,
                repository::GraphRepository& graph);

  Result<domain::IngestResult> Run();
  Result<domain::IngestResult> RunFromDatabase();

 private:
  Result<std::vector<domain::Author>> LoadAuthors();
  Result<std::vector<domain::Work>> LoadWorks(
      std::span<const domain::Author> authors);
  Result<domain::IngestResult> RunAlgorithms(
      std::span<const domain::Edge> edges,
      std::span<const domain::ArticleTopicInput> topic_inputs,
      domain::IngestResult result);

  [[nodiscard]] openalex::ClientOptions ClientOptions() const;

  const Config& config_;
  repository::IngestRepository& repository_;
  repository::CommunityRepository& communities_;
  repository::GraphRepository& graph_;
  RateLimiter limiter_;
};

}  // namespace schools::service

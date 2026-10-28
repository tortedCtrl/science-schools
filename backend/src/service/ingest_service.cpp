#include "service/ingest_service.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "graph_engine/leiden_wrapper.hpp"

namespace schools::service {
namespace {

using EdgeKey = std::pair<std::string, std::string>;

EdgeKey MakeEdgeKey(const std::string& first, const std::string& second) {
  return first < second ? EdgeKey{first, second} : EdgeKey{second, first};
}

Result<std::vector<domain::Membership>> ComputeMemberships(
    std::span<const domain::Edge> edges) {
  if (edges.empty()) {
    return std::vector<domain::Membership>{};
  }

  std::vector<graph::CoauthorshipEdge> input_edges;
  input_edges.reserve(edges.size());
  for (const domain::Edge& edge : edges) {
    input_edges.push_back({.source_id = static_cast<std::uint32_t>(edge.source),
                           .target_id = static_cast<std::uint32_t>(edge.target),
                           .weight = edge.weight});
  }

  try {
    const std::vector<std::uint32_t> communities =
        graph::LeidenWrapper::RunLeiden(input_edges, 1.25);
    std::vector<domain::Membership> memberships;
    memberships.reserve(communities.size());
    for (std::size_t index = 0; index < communities.size(); ++index) {
      memberships.push_back(
          {.author_id = static_cast<std::int32_t>(index),
           .community_id = static_cast<std::int32_t>(communities[index])});
    }
    return memberships;
  } catch (const std::exception& error) {
    return MakeError(Errc::kInternal, "Leiden clustering failed: {}",
                     error.what());
  }
}

}  // namespace

IngestService::IngestService(const Config& config,
                             repository::IngestRepository& repository,
                             repository::CommunityRepository& communities)
    : config_{config},
      repository_{repository},
      communities_{communities},
      limiter_{config.openalex_rps} {}

openalex::ClientOptions IngestService::ClientOptions() const {
  openalex::ClientOptions options{};
  options.mailto = config_.openalex_mailto;
  options.api_key = config_.openalex_api_key;
  options.max_retries = config_.openalex_max_retries;
  return options;
}

Result<std::vector<domain::Author>> IngestService::LoadAuthors() {
  const int page_size = std::max(config_.ingest_author_page_size, 1);
  const int page_count =
      std::max((config_.ingest_max_authors + page_size - 1) / page_size, 1);

  std::vector<std::vector<domain::Author>> pages(
      static_cast<std::size_t>(page_count));
  std::atomic<int> next_page{0};
  std::atomic<bool> failed{false};
  std::mutex error_mutex;
  std::optional<Error> error;

  const auto record_error = [&](Error value) {
    failed.store(true);
    const std::lock_guard<std::mutex> lock(error_mutex);
    if (!error.has_value()) {
      error = std::move(value);
    }
  };

  const int worker_count = std::min(config_.ingest_concurrency, page_count);
  std::vector<std::jthread> workers;
  workers.reserve(static_cast<std::size_t>(worker_count));

  for (int worker = 0; worker < worker_count; ++worker) {
    workers.emplace_back([&] {
      openalex::OpenAlexClient client(ClientOptions(), limiter_);
      while (!failed.load()) {
        const int page = next_page.fetch_add(1);
        if (page >= page_count) {
          return;
        }
        auto result = client.FetchAuthorsPage(page + 1, page_size);
        if (!result) {
          record_error(result.error());
          return;
        }
        pages[static_cast<std::size_t>(page)] = std::move(*result);
      }
    });
  }
  workers.clear();  // jthread destructors join all workers

  if (error.has_value()) {
    return std::unexpected(std::move(*error));
  }

  std::vector<domain::Author> authors;
  std::unordered_set<std::string> seen;
  authors.reserve(static_cast<std::size_t>(config_.ingest_max_authors));
  for (const auto& page : pages) {
    for (const domain::Author& author : page) {
      if (authors.size() >=
          static_cast<std::size_t>(config_.ingest_max_authors)) {
        return authors;
      }
      if (author.openalex_id.empty() ||
          !seen.insert(author.openalex_id).second) {
        continue;
      }
      authors.push_back(author);
    }
  }
  return authors;
}

Result<std::vector<domain::Work>> IngestService::LoadWorks(
    std::span<const domain::Author> authors) {
  const int batch_size = std::max(config_.ingest_work_batch_size, 1);
  std::vector<std::vector<std::string>> batches;
  for (std::size_t offset = 0; offset < authors.size();
       offset += static_cast<std::size_t>(batch_size)) {
    const std::size_t count =
        std::min(static_cast<std::size_t>(batch_size), authors.size() - offset);
    std::vector<std::string> ids;
    ids.reserve(count);
    for (const domain::Author& author : authors.subspan(offset, count)) {
      ids.push_back(author.openalex_id);
    }
    batches.push_back(std::move(ids));
  }

  std::unordered_set<std::string> selected_authors;
  selected_authors.reserve(authors.size());
  for (const domain::Author& author : authors) {
    selected_authors.insert(author.openalex_id);
  }

  std::unordered_map<std::string, domain::Work> works;
  std::mutex merge_mutex;
  std::atomic<std::size_t> next_batch{0};
  std::atomic<bool> failed{false};
  std::mutex error_mutex;
  std::optional<Error> error;

  const auto record_error = [&](Error value) {
    failed.store(true);
    const std::lock_guard<std::mutex> lock(error_mutex);
    if (!error.has_value()) {
      error = std::move(value);
    }
  };

  const int worker_count = std::min<int>(config_.ingest_concurrency,
                                         static_cast<int>(batches.size()));
  if (worker_count == 0) {
    return std::vector<domain::Work>{};
  }

  std::vector<std::jthread> workers;
  workers.reserve(static_cast<std::size_t>(worker_count));

  for (int worker = 0; worker < worker_count; ++worker) {
    workers.emplace_back([&] {
      openalex::OpenAlexClient client(ClientOptions(), limiter_);
      while (!failed.load()) {
        const std::size_t batch = next_batch.fetch_add(1);
        if (batch >= batches.size()) {
          return;
        }
        for (int page = 1; page <= config_.ingest_work_pages; ++page) {
          auto page_works =
              client.FetchWorksPage(batches[batch], selected_authors, page,
                                    config_.ingest_work_page_size);
          if (!page_works) {
            record_error(page_works.error());
            return;
          }

          const std::lock_guard<std::mutex> lock(merge_mutex);
          for (domain::Work& work : *page_works) {
            const std::string work_id = work.openalex_id;
            works.emplace(work_id, std::move(work));
          }
        }
      }
    });
  }
  workers.clear();

  if (error.has_value()) {
    return std::unexpected(std::move(*error));
  }

  std::vector<domain::Work> result;
  result.reserve(works.size());
  for (auto& [id, work] : works) {
    (void)id;
    result.push_back(std::move(work));
  }
  return result;
}

Result<domain::IngestResult> IngestService::Run() {
  spdlog::info("Ingest: fetching up to {} authors with {} workers and {} req/s",
               config_.ingest_max_authors, config_.ingest_concurrency,
               config_.openalex_rps);

  auto cleared = repository_.ClearImportData();
  if (!cleared) {
    return std::unexpected(cleared.error());
  }

  auto authors = LoadAuthors();
  if (!authors) {
    return std::unexpected(authors.error());
  }
  if (authors->empty()) {
    spdlog::warn("Ingest: no authors returned by OpenAlex");
    return domain::IngestResult{};
  }
  spdlog::info("Ingest: downloaded {} authors", authors->size());

  auto works = LoadWorks(*authors);
  if (!works) {
    return std::unexpected(works.error());
  }
  spdlog::info("Ingest: collected {} unique works", works->size());

  std::map<EdgeKey, std::int32_t> edge_weights;
  for (const domain::Work& work : *works) {
    for (std::size_t first = 0; first < work.author_ids.size(); ++first) {
      for (std::size_t second = first + 1; second < work.author_ids.size();
           ++second) {
        ++edge_weights[MakeEdgeKey(work.author_ids[first],
                                   work.author_ids[second])];
      }
    }
  }

  std::unordered_set<std::string> connected_authors;
  for (const auto& [pair, weight] : edge_weights) {
    (void)weight;
    connected_authors.insert(pair.first);
    connected_authors.insert(pair.second);
  }

  std::unordered_map<std::string, std::int32_t> dense_ids;
  std::vector<domain::Author> stored_authors;
  dense_ids.reserve(connected_authors.size());
  stored_authors.reserve(connected_authors.size());
  for (const domain::Author& author : *authors) {
    if (!connected_authors.contains(author.openalex_id)) {
      continue;
    }
    const auto dense_id = static_cast<std::int32_t>(stored_authors.size());
    dense_ids.emplace(author.openalex_id, dense_id);
    stored_authors.push_back(author);
  }

  std::vector<domain::Edge> edges;
  edges.reserve(edge_weights.size());
  for (const auto& [pair, weight] : edge_weights) {
    const auto source_it = dense_ids.find(pair.first);
    const auto target_it = dense_ids.find(pair.second);
    if (source_it == dense_ids.end() || target_it == dense_ids.end()) {
      continue;
    }
    std::int32_t source = source_it->second;
    std::int32_t target = target_it->second;
    if (source > target) {
      std::swap(source, target);
    }
    edges.push_back(
        domain::Edge{.source = source, .target = target, .weight = weight});
  }

  std::vector<domain::Article> articles;
  std::vector<domain::AuthorArticle> links;
  for (const domain::Work& work : *works) {
    bool needed = false;
    for (const std::string& author_id : work.author_ids) {
      if (dense_ids.contains(author_id)) {
        needed = true;
        break;
      }
    }
    if (!needed) {
      continue;
    }

    articles.push_back(domain::Article{.openalex_id = work.openalex_id,
                                       .title = work.title,
                                       .year = work.year});
    for (const std::string& author_id : work.author_ids) {
      const auto it = dense_ids.find(author_id);
      if (it != dense_ids.end()) {
        links.push_back(domain::AuthorArticle{.author_id = it->second,
                                              .article_id = work.openalex_id});
      }
    }
  }

  auto saved = repository_.SaveGraph(stored_authors, edges, articles, links);
  if (!saved) {
    return std::unexpected(saved.error());
  }

  auto memberships = ComputeMemberships(edges);
  if (!memberships) {
    return std::unexpected(memberships.error());
  }
  auto communities_saved = communities_.ReplaceMemberships(*memberships);
  if (!communities_saved) {
    return std::unexpected(communities_saved.error());
  }

  domain::IngestResult result{};
  result.authors = stored_authors.size();
  result.works = works->size();
  result.edges = edges.size();
  result.articles = articles.size();
  result.communities = memberships->size();
  spdlog::info(
      "Ingest: saved {} authors, {} edges, {} articles, {} communities",
      result.authors, result.edges, result.articles, result.communities);
  return result;
}

}  // namespace schools::service

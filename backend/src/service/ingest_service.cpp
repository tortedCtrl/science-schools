#include "service/ingest_service.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <limits>
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

#include "common/scoped_timer.hpp"
#include "graph_engine/bigclam_wrapper.hpp"
#include "graph_engine/leiden_wrapper.hpp"
#include "service/community_topic_aggregator.hpp"

namespace schools::service {
namespace {

using EdgeKey = std::pair<std::string, std::string>;

EdgeKey MakeEdgeKey(const std::string& first, const std::string& second) {
  return first < second ? EdgeKey{first, second} : EdgeKey{second, first};
}

Result<std::vector<domain::Membership>> ComputeLeidenMemberships(
    std::span<const domain::Edge> edges) {
  if (edges.empty()) {
    return std::vector<domain::Membership>{};
  }

  std::vector<graph::CoauthorshipEdge> input_edges;
  input_edges.reserve(edges.size());
  for (const domain::Edge& edge : edges) {
    if (edge.weight < 0 ||
        edge.weight > std::numeric_limits<std::int32_t>::max()) {
      return MakeError(Errc::kInvalidArgument,
                       "edge weight {} is outside the Leiden range",
                       edge.weight);
    }
    input_edges.push_back({.source_id = static_cast<std::uint32_t>(edge.source),
                           .target_id = static_cast<std::uint32_t>(edge.target),
                           .weight = static_cast<std::int32_t>(edge.weight)});
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

Result<std::vector<domain::Membership>> ComputeBigClamMemberships(
    std::span<const domain::Edge> edges, const Config& config) {
  if (edges.empty()) {
    return std::vector<domain::Membership>{};
  }
  return graph::BigClamWrapper::Run(
      edges, std::filesystem::path{config.bigclam_binary},
      config.bigclam_communities, config.bigclam_threads);
}

}  // namespace

IngestService::IngestService(const Config& config,
                             repository::IngestRepository& repository,
                             repository::CommunityRepository& communities,
                             repository::GraphRepository& graph)
    : config_{config},
      repository_{repository},
      communities_{communities},
      graph_{graph},
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
  std::atomic<int> completed_pages{0};
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
          const Error& fetch_error = result.error();
          record_error(Error{
              .code = fetch_error.code,
              .message = fmt::format("OpenAlex author page {} failed: {}",
                                     page + 1, fetch_error.message)});
          return;
        }
        pages[static_cast<std::size_t>(page)] = std::move(*result);
        const int completed = completed_pages.fetch_add(1) + 1;
        const int log_interval = std::max(page_count / 10, 1);
        if (completed == 1 || completed == page_count ||
            completed % log_interval == 0) {
          spdlog::info(
              "Download progress: completed author pages {}/{} (page {}: {} "
              "authors)",
              completed, page_count, page + 1,
              pages[static_cast<std::size_t>(page)].size());
        }
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
  std::atomic<std::size_t> completed_batches{0};
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
        std::size_t batch_work_count{0};
        for (int page = 1; page <= config_.ingest_work_pages; ++page) {
          auto page_works =
              client.FetchWorksPage(batches[batch], selected_authors, page,
                                    config_.ingest_work_page_size);
          if (!page_works) {
            const Error& fetch_error = page_works.error();
            record_error(Error{
                .code = fetch_error.code,
                .message = fmt::format(
                    "OpenAlex works batch {}/{} page {} failed: {}",
                    batch + 1, batches.size(), page, fetch_error.message)});
            return;
          }

          batch_work_count += page_works->size();
          const std::lock_guard<std::mutex> lock(merge_mutex);
          for (domain::Work& work : *page_works) {
            const std::string work_id = work.openalex_id;
            works.emplace(work_id, std::move(work));
          }
        }
        const std::size_t completed = completed_batches.fetch_add(1) + 1;
        const std::size_t log_interval =
            std::max(batches.size() / 10, std::size_t{1});
        if (completed == 1 || completed == batches.size() ||
            completed % log_interval == 0) {
          spdlog::info(
              "Download progress: completed work batches {}/{} (batch {}: {} "
              "works)",
              completed, batches.size(), batch + 1, batch_work_count);
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
  spdlog::info("Download started: fetching up to {} authors with {} workers "
               "and {} req/s",
               config_.ingest_max_authors, config_.ingest_concurrency,
               config_.openalex_rps);
  {
    const schools::ScopedTimer timer{"ClearImportData", config_.show_timer_logs};
    auto cleared = repository_.ClearImportData();
    if (!cleared) {
      return std::unexpected(cleared.error());
    }
  }

  std::vector<domain::Author> authors;
  {
    const schools::ScopedTimer timer{"LoadAuthors", config_.show_timer_logs};
    auto loaded = LoadAuthors();
    if (!loaded) {
      return std::unexpected(loaded.error());
    }
    authors = std::move(*loaded);
  }
  if (authors.empty()) {
    spdlog::warn("Download finished: OpenAlex returned no authors");
    spdlog::info("Processing finished: no authors to process");
    return domain::IngestResult{};
  }

  spdlog::info("Download progress: fetching works for {} selected authors",
               authors.size());
  spdlog::info("Ingest: downloaded {} authors", authors.size());

  std::vector<domain::Work> works;
  {
    const schools::ScopedTimer timer{"LoadWorks", config_.show_timer_logs};
    auto loaded = LoadWorks(authors);
    if (!loaded) {
      return std::unexpected(loaded.error());
    }
    works = std::move(*loaded);
  }
  spdlog::info("Ingest: collected {} unique works", works.size());

  std::vector<domain::Author> stored_authors;
  std::vector<domain::Edge> edges;
  std::vector<domain::Article> articles;
  std::vector<domain::AuthorArticle> links;
  std::vector<domain::ArticleTopicInput> topic_inputs;
  {
    const schools::ScopedTimer timer{"BuildGraphAndEdges",
                                     config_.show_timer_logs};
    std::map<EdgeKey, std::int32_t> edge_weights;
    for (const domain::Work& work : works) {
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
    dense_ids.reserve(connected_authors.size());
    stored_authors.reserve(connected_authors.size());
    for (const domain::Author& author : authors) {
      if (!connected_authors.contains(author.openalex_id)) {
        continue;
      }
      const auto dense_id = static_cast<std::int32_t>(stored_authors.size());
      dense_ids.emplace(author.openalex_id, dense_id);
      stored_authors.push_back(author);
    }

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

    for (const domain::Work& work : works) {
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

      domain::Article article{.openalex_id = work.openalex_id,
                              .title = work.title,
                              .year = work.year,
                              .topics = work.topics};
      domain::ArticleTopicInput topic_input{.article = article};
      for (const std::string& author_id : work.author_ids) {
        const auto it = dense_ids.find(author_id);
        if (it != dense_ids.end()) {
          links.push_back(domain::AuthorArticle{.author_id = it->second,
                                                .article_id = work.openalex_id});
          topic_input.author_ids.push_back(it->second);
        }
      }
      articles.push_back(std::move(article));
      topic_inputs.push_back(std::move(topic_input));
    }

    {
      const schools::ScopedTimer save_timer{"SaveGraph",
                                            config_.show_timer_logs};
      auto saved = repository_.SaveGraph(stored_authors, edges, articles, links);
      if (!saved) {
        return std::unexpected(saved.error());
      }
    }
  }

  domain::IngestResult result{};
  result.authors = stored_authors.size();
  result.works = works.size();
  result.edges = edges.size();
  result.articles = articles.size();
  spdlog::info("Download finished: {} authors, {} works, {} edges, {} articles",
               result.authors, result.works, result.edges, result.articles);
  return RunAlgorithms(edges, topic_inputs, result);
}

Result<domain::IngestResult> IngestService::RunFromDatabase() {
  spdlog::info("Download skipped: reusing existing PostgreSQL data");
  std::vector<domain::Edge> edges;
  {
    const schools::ScopedTimer timer{"LoadEdges", config_.show_timer_logs};
    auto loaded = graph_.ListEdges();
    if (!loaded) {
      return std::unexpected(loaded.error());
    }
    edges = std::move(*loaded);
  }

  std::vector<domain::ArticleTopicInput> topic_inputs;
  {
    const schools::ScopedTimer timer{"LoadArticleTopicInputs",
                                     config_.show_timer_logs};
    auto loaded = graph_.GetArticleTopicInputs();
    if (!loaded) {
      return std::unexpected(loaded.error());
    }
    topic_inputs = std::move(*loaded);
  }

  std::unordered_set<std::int32_t> author_ids;
  for (const domain::Edge& edge : edges) {
    author_ids.insert(edge.source);
    author_ids.insert(edge.target);
  }

  domain::IngestResult result{};
  result.authors = author_ids.size();
  result.works = topic_inputs.size();
  result.edges = edges.size();
  result.articles = topic_inputs.size();
  return RunAlgorithms(edges, topic_inputs, result);
}

Result<domain::IngestResult> IngestService::RunAlgorithms(
    std::span<const domain::Edge> edges,
    std::span<const domain::ArticleTopicInput> topic_inputs,
    domain::IngestResult result) {
  spdlog::info("Processing started: {} edges, {} articles", edges.size(),
               topic_inputs.size());
  spdlog::info("Algorithms to run: Leiden and SNAP BigCLAM");

  std::vector<domain::Membership> memberships;
  std::int64_t communities_saved_leiden = 0;
  {
    const schools::ScopedTimer timer{"LeidenClustering",
                                     config_.show_timer_logs};
    spdlog::info("Starting Leiden algorithm on {} edges", edges.size());
    auto memberships_result = ComputeLeidenMemberships(edges);
    if (!memberships_result) {
      return std::unexpected(memberships_result.error());
    }
    memberships = std::move(*memberships_result);
  }

  {
    const schools::ScopedTimer timer{"CommunityTopicsLeiden",
                                     config_.show_timer_logs};
    spdlog::info("Starting topic calculation for Leiden clusters");
    const std::vector<domain::CommunityTopic> community_topics =
        ComputeCommunityTopics(memberships, topic_inputs,
                               domain::CommunityAlgorithm::kLeiden);
    {
      const schools::ScopedTimer save_timer{"SaveLeidenResults",
                                            config_.show_timer_logs};
      auto saved =
          communities_.SaveLeidenResults(memberships, community_topics);
      if (!saved) {
        return std::unexpected(saved.error());
      }
      communities_saved_leiden = *saved;
    }
  }

  std::vector<domain::Membership> bigclam_memberships;
  std::int64_t bigclam_saved = 0;
  {
    const schools::ScopedTimer timer{"BigClamRun", config_.show_timer_logs};
    spdlog::info(
        "Starting SNAP BigCLAM algorithm on {} edges "
        "(communities={}, threads={})",
        edges.size(), config_.bigclam_communities, config_.bigclam_threads);
    auto bigclam_result = ComputeBigClamMemberships(edges, config_);
    if (!bigclam_result) {
      return std::unexpected(bigclam_result.error());
    }
    bigclam_memberships = std::move(*bigclam_result);
  }

  {
    const schools::ScopedTimer timer{"CommunityTopicsBigClam",
                                     config_.show_timer_logs};
    spdlog::info("Starting topic calculation for SNAP BigCLAM clusters");
    const std::vector<domain::CommunityTopic> bigclam_topics =
        ComputeCommunityTopics(bigclam_memberships, topic_inputs,
                               domain::CommunityAlgorithm::kBigClam);
    {
      const schools::ScopedTimer save_timer{"SaveBigClamResults",
                                            config_.show_timer_logs};
      auto saved =
          communities_.SaveBigClamResults(bigclam_memberships, bigclam_topics);
      if (!saved) {
        return std::unexpected(saved.error());
      }
      bigclam_saved = *saved;
    }
  }

  result.communities = memberships.size();
  spdlog::info(
      "Ingest: saved Leiden run {} ({} memberships) and BigCLAM run {} "
      "({} memberships); {} authors, {} edges, {} articles",
      communities_saved_leiden, memberships.size(), bigclam_saved,
      bigclam_memberships.size(), result.authors, result.edges,
      result.articles);
  spdlog::info("Processing finished");
  return result;
}

}  // namespace schools::service

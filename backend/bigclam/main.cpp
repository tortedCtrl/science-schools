#include <spdlog/spdlog.h>

#include <charconv>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_set>

#include "common/config.hpp"
#include "graph_engine/bigclam_wrapper.hpp"
#include "repository/community_repository.hpp"
#include "repository/database.hpp"
#include "repository/graph_repository.hpp"
#include "repository/schema.hpp"
#include "service/community_topic_aggregator.hpp"

namespace {

bool ParsePositive(std::string_view text, int& output) {
  int value = 0;
  const auto [end, error] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value <= 0) {
    return false;
  }
  output = value;
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  int communities = 10;
  int threads = 1;
  std::string executable = "/usr/local/bin/bigclam";
  if (const char* env = std::getenv("BIGCLAM_BINARY")) {
    executable = env;
  }
  for (int i = 1; i < argc; ++i) {
    const std::string_view option = argv[i];
    if (option == "--help") {
      spdlog::info("Usage: bigclam_runner [--communities N] [--threads N] "
                   "[--binary /path/to/bigclam]");
      return 0;
    }
    if (i + 1 >= argc) {
      spdlog::error("Missing value for {}", option);
      return 2;
    }
    const std::string_view value = argv[++i];
    if (option == "--communities") {
      if (!ParsePositive(value, communities)) {
        spdlog::error("Invalid number of communities: {}", value);
        return 2;
      }
    } else if (option == "--threads") {
      if (!ParsePositive(value, threads)) {
        spdlog::error("Invalid number of threads: {}", value);
        return 2;
      }
    } else if (option == "--binary") {
      executable = value;
    } else {
      spdlog::error("Unknown option: {}", option);
      return 2;
    }
  }

  auto config = schools::Config::FromEnv();
  if (!config) {
    spdlog::error("{}", config.error().message);
    return 1;
  }
  schools::repository::Database database;
  if (auto opened = database.Open(config->db_connection, 1); !opened) {
    spdlog::error("{}", opened.error().message);
    return 1;
  }
  if (auto schema = schools::repository::EnsureSchema(database); !schema) {
    spdlog::error("{}", schema.error().message);
    return 1;
  }

  schools::repository::GraphRepository graphs(database);
  auto edges = graphs.ListEdges();
  if (!edges) {
    spdlog::error("{}", edges.error().message);
    return 1;
  }
  spdlog::info("Running SNAP BigCLAM: edges={}, communities={}, threads={}",
               edges->size(), communities, threads);
  auto memberships = schools::graph::BigClamWrapper::Run(
      *edges, executable, communities, threads);
  if (!memberships) {
    spdlog::error("{}", memberships.error().message);
    return 1;
  }

  auto article_topic_inputs = graphs.GetArticleTopicInputs();
  if (!article_topic_inputs) {
    spdlog::error("{}", article_topic_inputs.error().message);
    return 1;
  }
  const auto topics = schools::service::ComputeCommunityTopics(
      *memberships, *article_topic_inputs,
      schools::domain::CommunityAlgorithm::kBigClam);

  schools::repository::CommunityRepository repository(database);
  if (auto saved = repository.ReplaceBigClamResults(*memberships, topics);
      !saved) {
    spdlog::error("{}", saved.error().message);
    return 1;
  }
  std::unordered_set<int> unique_authors;
  std::unordered_set<int> unique_communities;
  for (const auto& membership : *memberships) {
    unique_authors.insert(membership.author_id);
    unique_communities.insert(membership.community_id);
  }
  spdlog::info("BigCLAM saved: {} memberships, {} authors, {} communities",
               memberships->size(), unique_authors.size(),
               unique_communities.size());
  return 0;
}

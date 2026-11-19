#include "graph_engine/bigclam_wrapper.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace schools::graph {
namespace {

namespace fs = std::filesystem;

// Every invocation gets its own workspace, including its own result file.
class TemporaryDirectory {
 public:
  TemporaryDirectory() = default;
  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
  ~TemporaryDirectory() {
    if (!path_.empty()) {
      std::error_code ignored;
      fs::remove_all(path_, ignored);
    }
  }

  Status Create() {
    std::error_code error;
    const fs::path parent = fs::temp_directory_path(error);
    if (error) {
      return MakeError(Errc::kInternal, "find temporary directory: {}",
                       error.message());
    }
    std::string name = (parent / "science-bigclam-XXXXXX").string();
    if (mkdtemp(name.data()) == nullptr) {
      return MakeError(Errc::kInternal, "create temporary directory: errno {}",
                       errno);
    }
    path_ = name;
    return {};
  }

  const fs::path& Path() const { return path_; }

 private:
  fs::path path_;
};

Status RunExecutable(const fs::path& binary, const fs::path& edges,
                     const fs::path& labels, const fs::path& prefix,
                     int communities, int threads) {
  // No shell is involved: arguments cannot be interpreted as shell commands.
  const std::string executable = binary.string();
  const std::string input_arg = "-i:" + edges.string();
  const std::string label_arg = "-l:" + labels.string();
  const std::string output_arg = "-o:" + prefix.string();
  const std::string community_arg = "-c:" + std::to_string(communities);
  const std::string threads_arg = "-nt:" + std::to_string(threads);

  const pid_t child = fork();
  if (child < 0) {
    return MakeError(Errc::kInternal, "fork BigCLAM: errno {}", errno);
  }
  if (child == 0) {
    execl(executable.c_str(), executable.c_str(), input_arg.c_str(),
          label_arg.c_str(), output_arg.c_str(), community_arg.c_str(),
          threads_arg.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }

  int status = 0;
  pid_t waited;
  do {
    waited = waitpid(child, &status, 0);
  } while (waited == -1 && errno == EINTR);
  if (waited == -1) {
    return MakeError(Errc::kInternal, "wait for BigCLAM: errno {}", errno);
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return MakeError(Errc::kInternal, "BigCLAM failed (exit status {})",
                     WIFEXITED(status) ? WEXITSTATUS(status) : -1);
  }
  return {};
}

Result<std::vector<domain::Membership>> ParseCommunities(
    const fs::path& file, const std::set<std::int32_t>& valid_ids) {
  std::ifstream input(file);
  if (!input) {
    return MakeError(Errc::kInternal, "BigCLAM output file is missing: {}",
                     file.string());
  }

  std::vector<domain::Membership> memberships;
  std::set<std::pair<std::int32_t, std::int32_t>> unique_memberships;
  std::string line;
  std::int32_t community_id = 0;
  while (std::getline(input, line)) {
    std::istringstream tokens(line);
    std::string token;
    while (tokens >> token) {
      std::int32_t author_id = -1;
      const auto [end, error] =
          std::from_chars(token.data(), token.data() + token.size(), author_id);
      if (error != std::errc{} || end != token.data() + token.size() ||
          !valid_ids.contains(author_id)) {
        return MakeError(Errc::kParse,
                         "BigCLAM returned an invalid author ID: {}", token);
      }
      if (unique_memberships.emplace(author_id, community_id).second) {
        memberships.push_back(
            {.author_id = author_id, .community_id = community_id});
      }
    }
    if (community_id == std::numeric_limits<std::int32_t>::max()) {
      return MakeError(Errc::kParse, "too many BigCLAM communities");
    }
    ++community_id;
  }
  if (input.bad() || memberships.empty()) {
    return MakeError(Errc::kParse, "BigCLAM returned no valid memberships");
  }
  return memberships;
}

}  // namespace

Result<std::vector<domain::Membership>> BigClamWrapper::Run(
    std::span<const domain::Edge> edges, const fs::path& executable,
    int communities, int threads) {
  if (communities <= 0 || threads <= 0) {
    return MakeError(Errc::kInvalidArgument,
                     "communities and threads must be positive");
  }
  if (edges.empty()) {
    return MakeError(Errc::kInvalidArgument,
                     "coauthorship graph has no edges");
  }
  if (!fs::exists(executable)) {
    return MakeError(Errc::kNotFound,
                     "BigCLAM executable not found: {}", executable.string());
  }

  TemporaryDirectory temporary;
  auto created = temporary.Create();
  if (!created) {
    return std::unexpected(created.error());
  }
  const fs::path edge_file = temporary.Path() / "edges.txt";
  const fs::path label_file = temporary.Path() / "labels.tsv";
  const fs::path prefix = temporary.Path() / "result_";

  std::set<std::int32_t> ids;
  {
    std::ofstream output(edge_file);
    if (!output) {
      return MakeError(Errc::kInternal, "cannot create BigCLAM edge list");
    }
    for (const domain::Edge& edge : edges) {
      if (edge.source < 0 || edge.target < 0 || edge.source == edge.target ||
          edge.weight <= 0) {
        return MakeError(Errc::kInvalidArgument,
                         "invalid edge {} - {} (weight {})", edge.source,
                         edge.target, edge.weight);
      }
      // SNAP BigCLAM operates on an unweighted graph: one pair per edge.
      output << edge.source << '\t' << edge.target << '\n';
      ids.insert(edge.source);
      ids.insert(edge.target);
    }
    output.close();
    if (!output) {
      return MakeError(Errc::kInternal, "cannot write BigCLAM edge list");
    }
  }
  {
    std::ofstream output(label_file);
    if (!output) {
      return MakeError(Errc::kInternal, "cannot create BigCLAM labels");
    }
    // The -l: option forces numeric node IDs to be preserved. Without it,
    // SNAP's string loader would internally renumber nodes.
    for (const std::int32_t id : ids) {
      output << id << '\t' << id << '\n';
    }
    output.close();
    if (!output) {
      return MakeError(Errc::kInternal, "cannot write BigCLAM labels");
    }
  }

  auto run = RunExecutable(executable, edge_file, label_file, prefix,
                           communities, threads);
  if (!run) {
    return std::unexpected(run.error());
  }
  return ParseCommunities(temporary.Path() / "result_cmtyvv.txt", ids);
}

}  // namespace schools::graph

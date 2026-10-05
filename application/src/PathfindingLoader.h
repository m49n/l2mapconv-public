#pragma once
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <pathfinding/Dataset.h>
struct PathfindingLoadRequest {
  std::string map;
  std::filesystem::path l2j, navmesh;
  std::optional<pathfinding::RouteCase> saved_case;
  std::vector<std::filesystem::path> nav_regions{};
};
struct PathfindingLoadResult {
  std::uint64_t generation{};
  std::shared_ptr<const pathfinding::Dataset> dataset;
  std::string error;
};
class PathfindingLoader {
public:
  using Load = std::function<std::shared_ptr<const pathfinding::Dataset>(
      const PathfindingLoadRequest &)>;
  explicit PathfindingLoader(Load load = {});
  void request(PathfindingLoadRequest);
  void invalidate();
  std::optional<PathfindingLoadResult> poll();
  bool active() const;

private:
  Load m_load;
  std::uint64_t m_generation{};
  std::optional<PathfindingLoadRequest> m_pending;
  std::future<PathfindingLoadResult> m_future;
};

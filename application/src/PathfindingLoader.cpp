#include "PathfindingLoader.h"
#include <navmesh/RegionMetadata.h>
PathfindingLoader::PathfindingLoader(Load load) : m_load(std::move(load)) {
  if (!m_load)
    m_load = [](const PathfindingLoadRequest &r) {
      const auto a = r.saved_case    ? r.saved_case->l2j
                     : r.l2j.empty() ? pathfinding::FileIdentity{}
                                     : pathfinding::identify_file(r.l2j);
      if(r.saved_case && !r.saved_case->nav_regions.empty())
        return std::make_shared<pathfinding::Dataset>(pathfinding::Dataset::load(r.saved_case->map,a,r.saved_case->nav_regions));
      if(!r.saved_case && !r.nav_regions.empty()) {
        std::vector<pathfinding::NavRegionInput> inputs;
        for(const auto& file:r.nav_regions) {
          const auto mesh=pathfinding::identify_file(file);
          if(!std::filesystem::exists(navmesh::metadata_path(file))) {
            if(r.nav_regions.size()!=1)throw std::invalid_argument("Legacy Navmesh has no sidecar; regenerate before combining regions");
            return std::make_shared<pathfinding::Dataset>(pathfinding::Dataset::load(r.map,a,mesh));
          }
          const auto marker=pathfinding::identify_file(navmesh::metadata_path(mesh.path));
          const auto metadata=navmesh::read_region_metadata(mesh.path);
          inputs.push_back({metadata.map,mesh,marker});
        }
        return std::make_shared<pathfinding::Dataset>(pathfinding::Dataset::load(r.map,a,inputs));
      }
      const auto b = r.saved_case ? r.saved_case->navmesh
                     : r.navmesh.empty()
                         ? pathfinding::FileIdentity{}
                         : pathfinding::identify_file(r.navmesh);
      return std::make_shared<pathfinding::Dataset>(
          pathfinding::Dataset::load(r.map, a, b));
    };
}
void PathfindingLoader::request(PathfindingLoadRequest r) {
  ++m_generation;
  m_pending = std::move(r);
}
void PathfindingLoader::invalidate() {
  ++m_generation;
  m_pending.reset();
}
std::optional<PathfindingLoadResult> PathfindingLoader::poll() {
  std::optional<PathfindingLoadResult> ready;
  if (m_future.valid() &&
      m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    auto r = m_future.get();
    if (r.generation == m_generation)
      ready = std::move(r);
  }
  if (!m_future.valid() && m_pending) {
    auto request = std::move(*m_pending);
    m_pending.reset();
    m_future = std::async(std::launch::async,
                          [load = m_load, request = std::move(request),
                           generation = m_generation] {
                            PathfindingLoadResult result;
                            result.generation = generation;
                            try {
                              result.dataset = load(request);
                            } catch (const std::exception &e) {
                              result.error = e.what();
                            }
                            return result;
                          });
  }
  return ready;
}
bool PathfindingLoader::active() const {
  return m_pending.has_value() || m_future.valid();
}

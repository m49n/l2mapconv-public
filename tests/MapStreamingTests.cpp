#include "MapSceneSink.h"
#include "MapStreamingSystem.h"
#include "TestSupport.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

class StreamingCatalogFixture {
public:
  StreamingCatalogFixture() {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    root = std::filesystem::temp_directory_path() /
           ("l2mapconv-map-streaming-" + suffix);
    std::filesystem::create_directories(root / "Maps");
    touch("22_22.unr");
    touch("22_23.unr");
    touch("23_22.unr");
    touch("24_22.unr");
    touch("25_22.unr");
  }

  ~StreamingCatalogFixture() { std::filesystem::remove_all(root); }

  void touch(const char *name) const {
    std::ofstream file{root / "Maps" / name};
  }

  std::filesystem::path root;
};

auto loaded_map(MapCoordinate coordinate) -> Map {
  const auto x = static_cast<float>(coordinate.x - 22) * 100.0f;
  const auto y = static_cast<float>(coordinate.y - 22) * 100.0f;
  Map map;
  map.name = std::to_string(coordinate.x) + "_" + std::to_string(coordinate.y);
  map.position = {x, y, 0.0f};
  map.bounding_box =
      geometry::Box{{x, y, 0.0f}, {x + 100.0f, y + 100.0f, 50.0f}};
  return map;
}

class ImmediateMapLoadService final : public MapLoadService {
public:
  void reconcile(std::vector<MapLoadRequest> requests,
                 std::vector<MapLoadKey> cancellations) override {
    for (const auto key : cancellations) {
      latest.erase(key);
    }
    last_requests = requests;
    for (const auto &request : requests) {
      latest.insert_or_assign(request.key, request.generation);
      started.push_back(request);
      if (auto_complete) {
        completed.push_back({request, loaded_map(request.key.coordinate), {}});
      }
    }
  }

  auto take_started() -> std::vector<MapLoadRequest> override {
    std::vector<MapLoadRequest> result;
    result.swap(started);
    return result;
  }

  auto take_completed() -> std::vector<MapLoadResult> override {
    std::vector<MapLoadResult> result;
    result.swap(completed);
    return result;
  }

  auto is_current(const MapLoadRequest &request) const -> bool override {
    const auto found = latest.find(request.key);
    return found != latest.end() && found->second == request.generation;
  }

  void inject(MapLoadRequest request) {
    completed.push_back({request, loaded_map(request.key.coordinate), {}});
  }

  bool auto_complete{true};
  std::vector<MapLoadRequest> last_requests;

private:
  std::map<MapLoadKey, std::uint64_t> latest;
  std::vector<MapLoadRequest> started;
  std::vector<MapLoadResult> completed;
};

class FakeMapSceneSink final : public MapSceneSink {
public:
  auto upload(MapCoordinate coordinate, MapLayer layer, const Map &)
      -> rendering::SceneGroupId override {
    const auto group = next_group++;
    groups.insert_or_assign(group, MapLoadKey{coordinate, layer});
    uploads.push_back({coordinate, layer});
    return group;
  }

  void remove(rendering::SceneGroupId group) override {
    const auto found = groups.find(group);
    if (found != groups.end()) {
      removed.push_back(found->second);
      groups.erase(found);
    }
  }

  auto camera_xy() const -> glm::vec2 override { return camera; }

  void place_camera_for_seed(const Map &map) override {
    camera = {(map.bounding_box.min().x + map.bounding_box.max().x) * 0.5f,
              (map.bounding_box.min().y + map.bounding_box.max().y) * 0.5f};
  }

  void set_region(MapCoordinate coordinate) {
    camera = {static_cast<float>(coordinate.x - 22) * 100.0f + 50.0f,
              static_cast<float>(coordinate.y - 22) * 100.0f + 50.0f};
  }

  auto removed_detail(MapCoordinate coordinate) const -> bool {
    return std::find(removed.begin(), removed.end(),
                     MapLoadKey{coordinate, MapLayer::Detail}) != removed.end();
  }

  std::size_t upload_count() const { return uploads.size(); }

private:
  glm::vec2 camera{};
  rendering::SceneGroupId next_group{1};
  std::map<rendering::SceneGroupId, MapLoadKey> groups;
  std::vector<MapLoadKey> uploads;
  std::vector<MapLoadKey> removed;
};

} // namespace

auto run_map_streaming_tests() -> int {
  auto failures = 0;
  const StreamingCatalogFixture fixture;
  MapSelectionContext selection{MapCatalog::discover(fixture.root)};
  selection.set_manual({22, 22}, true);
  selection.set_include_neighbors(false);

  auto service = std::make_unique<ImmediateMapLoadService>();
  auto *service_view = service.get();
  auto sink = std::make_unique<FakeMapSceneSink>();
  auto *sink_view = sink.get();
  MapStreamingSystem streaming{
      selection, std::move(service), std::move(sink), {22, 22}};

  streaming.tick();
  streaming.tick();
  streaming.tick();
  auto resident = streaming.resident();
  failures += expect(resident.detail == Coordinates{{22, 22}},
                     "current-only startup loads one detail map");

  sink_view->set_region({23, 22});
  streaming.tick();
  streaming.tick();
  resident = streaming.resident();
  failures += expect(sink_view->removed_detail({22, 22}),
                     "old automatic detail unloads after crossing");
  failures += expect(resident.terrain.contains({22, 22}),
                     "manual terrain survives detail downgrade");
  failures += expect(resident.detail.contains({23, 22}),
                     "new current detail becomes resident");

  selection.set_include_neighbors(true);
  streaming.tick();
  streaming.tick();
  resident = streaming.resident();
  failures += expect(resident.detail.size() <= 9 &&
                         resident.detail.size() ==
                             selection.catalog().neighbors({23, 22}, 1).size(),
                     "plus-one requests only available maps at a catalog edge");

  service_view->auto_complete = false;
  selection.set_include_neighbors(false);
  sink_view->set_region({25, 22});
  streaming.tick();
  const auto stale = std::find_if(
      service_view->last_requests.begin(), service_view->last_requests.end(),
      [](const MapLoadRequest &request) {
        return request.key.coordinate == MapCoordinate{25, 22} &&
               request.key.layer == MapLayer::Detail;
      });
  failures += expect(stale != service_view->last_requests.end(),
                     "crossing queues the new current detail");
  if (stale != service_view->last_requests.end()) {
    const auto stale_request = *stale;
    selection.set_auto_load(false);
    streaming.tick();
    service_view->inject(stale_request);
    const auto uploads_before = sink_view->upload_count();
    streaming.tick();
    failures += expect(sink_view->upload_count() == uploads_before,
                       "stale completion never reaches the renderer");
  }

  return failures;
}

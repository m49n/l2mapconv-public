#include "MapSceneSink.h"
#include "MapLoadPayload.h"
#include "MapStreamingSystem.h"
#include "TestSupport.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
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
      if (defer_start) {
        continue;
      }
      started.push_back(request);
      if (auto_complete) {
        auto payload = MapLoadPayload{loaded_map(request.key.coordinate), {}};
        if (request.key.layer == MapLayer::Detail) {
          payload.visual = std::make_shared<territory::VisualScene>();
        }
        completed.push_back({request, std::move(payload), {}});
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
    completed.push_back({request,
                         MapLoadPayload{loaded_map(request.key.coordinate),
                                        std::make_shared<territory::VisualScene>()},
                         {}});
  }

  void inject_failure(MapLoadRequest request, std::string error) {
    completed.push_back({std::move(request), std::nullopt, std::move(error)});
  }

  bool auto_complete{true};
  bool defer_start{};
  std::vector<MapLoadRequest> last_requests;

private:
  std::map<MapLoadKey, std::uint64_t> latest;
  std::vector<MapLoadRequest> started;
  std::vector<MapLoadResult> completed;
};

class FakeMapSceneSink final : public MapSceneSink {
public:
  auto upload(MapCoordinate coordinate, MapLayer layer,
              const MapLoadPayload &payload)
      -> rendering::SceneGroupId override {
    if (throw_on_detail && layer == MapLayer::Detail) {
      throw std::runtime_error{"fixture upload failure"};
    }
    if (layer == MapLayer::Detail && !payload.visual) {
      throw std::runtime_error{"detail has no visual payload"};
    }
    const auto group = next_group++;
    groups.insert_or_assign(group, MapLoadKey{coordinate, layer});
    visible.insert_or_assign(group, true);
    uploads.push_back({coordinate, layer});
    return group;
  }

  void remove(rendering::SceneGroupId group) override {
    const auto found = groups.find(group);
    if (found != groups.end()) {
      removed.push_back(found->second);
      groups.erase(found);
      visible.erase(group);
    }
  }

  void set_visible(rendering::SceneGroupId group, bool show) override {
    visible.at(group) = show;
  }

  auto layer_visible(MapCoordinate coordinate, MapLayer layer) const -> bool {
    for (const auto &[group, key] : groups) {
      if (key == MapLoadKey{coordinate, layer}) {
        return visible.at(group);
      }
    }
    return false;
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
  bool throw_on_detail{};

private:
  glm::vec2 camera{};
  rendering::SceneGroupId next_group{1};
  std::map<rendering::SceneGroupId, MapLoadKey> groups;
  std::map<rendering::SceneGroupId, bool> visible;
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
  failures += expect(!sink_view->layer_visible({22, 22}, MapLayer::Terrain) &&
                         sink_view->layer_visible({22, 22}, MapLayer::Detail),
                     "resident detail hides only its own terrain fallback");
  const auto uploads_before_manual_mode = sink_view->upload_count();
  selection.set_auto_load(false);
  streaming.tick();
  failures += expect(streaming.resident().detail == Coordinates{{22, 22}} &&
                         sink_view->layer_visible({22, 22}, MapLayer::Detail) &&
                         sink_view->upload_count() == uploads_before_manual_mode,
                     "disabling automatic loading keeps the checked current map visible without reupload");
  selection.set_auto_load(true);

  sink_view->set_region({23, 22});
  streaming.tick();
  streaming.tick();
  resident = streaming.resident();
  failures += expect(sink_view->removed_detail({22, 22}),
                     "old automatic detail unloads after crossing");
  failures += expect(resident.terrain.contains({22, 22}),
                     "manual terrain survives detail downgrade");
  failures += expect(sink_view->layer_visible({22, 22}, MapLayer::Terrain),
                     "removing detail restores retained terrain fallback");
  failures += expect(resident.detail.contains({23, 22}),
                     "new current detail becomes resident");

  MapSelectionContext manual_selection{MapCatalog::discover(fixture.root)};
  manual_selection.set_manual({22, 22}, true);
  manual_selection.set_manual({22, 23}, true);
  manual_selection.set_auto_load(false);
  manual_selection.set_include_neighbors(true);
  auto manual_service = std::make_unique<ImmediateMapLoadService>();
  auto manual_sink = std::make_unique<FakeMapSceneSink>();
  auto *manual_sink_view = manual_sink.get();
  MapStreamingSystem manual_streaming{manual_selection,
                                      std::move(manual_service),
                                      std::move(manual_sink), {22, 22}};
  manual_streaming.tick();
  manual_streaming.tick();
  manual_streaming.tick();
  auto manual_resident = manual_streaming.resident();
  failures += expect(manual_resident.detail == Coordinates{{22, 22}} &&
                         manual_resident.terrain == Coordinates{{22, 22}, {22, 23}} &&
                         !manual_sink_view->layer_visible({22, 22}, MapLayer::Terrain),
                     "manual mode loads selected current detail while selected neighbors remain terrain");
  manual_sink_view->set_region({23, 22});
  manual_streaming.tick();
  failures += expect(manual_streaming.resident().detail.empty() &&
                         manual_sink_view->layer_visible({22, 22}, MapLayer::Terrain),
                     "manual mode restores terrain when the camera enters an unchecked map");
  manual_selection.set_manual({23, 22}, true);
  manual_streaming.tick();
  manual_streaming.tick();
  failures += expect(manual_streaming.resident().detail == Coordinates{{23, 22}},
                     "checking the current map loads its full detail without automatic loading");
  manual_selection.set_manual({23, 22}, false);
  manual_streaming.tick();
  failures += expect(manual_streaming.resident().detail.empty() &&
                         !manual_streaming.resident().terrain.contains({23, 22}),
                     "unchecking the manual current map unloads its detail and terrain");
  manual_sink_view->set_region({99, 99});
  manual_streaming.tick();
  failures += expect(manual_streaming.resident().detail.empty(),
                     "manual mode keeps no detail when the camera is outside the catalog");

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

  MapSelectionContext priority_selection{MapCatalog::discover(fixture.root)};
  priority_selection.set_manual({22, 22}, true);
  priority_selection.set_include_neighbors(false);
  auto priority_service = std::make_unique<ImmediateMapLoadService>();
  auto *priority_service_view = priority_service.get();
  auto priority_sink = std::make_unique<FakeMapSceneSink>();
  auto *priority_sink_view = priority_sink.get();
  MapStreamingSystem priority_streaming{priority_selection,
                                        std::move(priority_service),
                                        std::move(priority_sink),
                                        {22, 22}};
  priority_streaming.tick();
  priority_streaming.tick();
  priority_streaming.tick();

  priority_service_view->auto_complete = false;
  priority_service_view->defer_start = true;
  priority_selection.select_all_manual();
  priority_selection.set_include_neighbors(true);
  priority_streaming.tick();
  priority_sink_view->set_region({23, 22});
  priority_streaming.tick();

  const auto reprioritized_current = std::find_if(
      priority_service_view->last_requests.begin(),
      priority_service_view->last_requests.end(),
      [](const MapLoadRequest &request) {
        return request.key == MapLoadKey{{23, 22}, MapLayer::Detail};
      });
  failures += expect(
      reprioritized_current != priority_service_view->last_requests.end() &&
          reprioritized_current->priority == MapLoadPriority::CurrentDetail,
      "queued detail is reprioritized after the camera crosses a boundary");

  const auto reprioritized_terrain = std::find_if(
      priority_service_view->last_requests.begin(),
      priority_service_view->last_requests.end(),
      [](const MapLoadRequest &request) {
        return request.key == MapLoadKey{{24, 22}, MapLayer::Terrain};
      });
  failures += expect(
      reprioritized_terrain != priority_service_view->last_requests.end() &&
          reprioritized_terrain->priority == MapLoadPriority::AutomaticTerrain,
      "queued manual terrain is reprioritized inside the automatic ring");

  MapSelectionContext retry_selection{MapCatalog::discover(fixture.root)};
  retry_selection.set_manual({22, 22}, true);
  retry_selection.set_include_neighbors(false);
  auto retry_service = std::make_unique<ImmediateMapLoadService>();
  auto *retry_service_view = retry_service.get();
  auto retry_sink = std::make_unique<FakeMapSceneSink>();
  auto *retry_sink_view = retry_sink.get();
  MapStreamingSystem retry_streaming{retry_selection,
                                     std::move(retry_service),
                                     std::move(retry_sink),
                                     {22, 22}};
  retry_streaming.tick();
  retry_streaming.tick();
  retry_streaming.tick();

  retry_service_view->auto_complete = false;
  retry_sink_view->set_region({25, 22});
  retry_streaming.tick();
  const auto failed_request = std::find_if(
      retry_service_view->last_requests.begin(),
      retry_service_view->last_requests.end(),
      [](const MapLoadRequest &request) {
        return request.key == MapLoadKey{{25, 22}, MapLayer::Detail};
      });
  failures += expect(failed_request != retry_service_view->last_requests.end(),
                     "new current detail can enter the loading queue");
  if (failed_request != retry_service_view->last_requests.end()) {
    const auto failed_generation = failed_request->generation;
    retry_service_view->inject_failure(*failed_request, "fixture failure");
    retry_streaming.tick();
    failures += expect(retry_selection.status({25, 22}, MapLayer::Detail) ==
                           MapResidencyStatus::Failed,
                       "failed current detail is visible in selection state");

    retry_selection.set_manual({25, 22}, false);
    retry_selection.set_manual({25, 22}, true);
    retry_streaming.tick();
    const auto retried = std::find_if(
        retry_service_view->last_requests.begin(),
        retry_service_view->last_requests.end(),
        [](const MapLoadRequest &request) {
          return request.key == MapLoadKey{{25, 22}, MapLayer::Detail};
        });
    failures +=
        expect(retried != retry_service_view->last_requests.end() &&
                   retried->generation > failed_generation,
               "reselecting a failed automatic map queues a fresh generation");
  }

  MapSelectionContext upload_selection{MapCatalog::discover(fixture.root)};
  upload_selection.set_manual({22, 22}, true);
  auto upload_service = std::make_unique<ImmediateMapLoadService>();
  auto upload_sink = std::make_unique<FakeMapSceneSink>();
  auto *upload_sink_view = upload_sink.get();
  upload_sink_view->throw_on_detail = true;
  MapStreamingSystem upload_streaming{upload_selection, std::move(upload_service),
                                      std::move(upload_sink), {22, 22}};
  upload_streaming.tick();
  upload_streaming.tick();
  upload_streaming.tick();
  failures += expect(upload_selection.status({22, 22}, MapLayer::Detail) ==
                         MapResidencyStatus::Failed &&
                         upload_sink_view->layer_visible({22, 22}, MapLayer::Terrain),
                     "failed detail GPU upload leaves terrain visible and reports failure");

  return failures;
}

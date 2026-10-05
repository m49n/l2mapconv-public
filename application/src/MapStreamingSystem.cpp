#include "MapStreamingSystem.h"

#include <utils/Log.h>

#include <string>
#include <utility>
#include <vector>

namespace {

auto layer_name(MapLayer layer) -> const char * {
  return layer == MapLayer::Terrain ? "terrain" : "detail";
}

auto status_name(MapResidencyStatus status) -> const char * {
  switch (status) {
  case MapResidencyStatus::NotResident:
    return "not_resident";
  case MapResidencyStatus::Queued:
    return "queued";
  case MapResidencyStatus::Loading:
    return "loading";
  case MapResidencyStatus::Resident:
    return "resident";
  case MapResidencyStatus::Failed:
    return "failed";
  }
  return "unknown";
}

} // namespace

MapStreamingSystem::MapStreamingSystem(MapSelectionContext &selection,
                                       std::unique_ptr<MapLoadService> loader,
                                       std::unique_ptr<MapSceneSink> scene,
                                       MapCoordinate seed_coordinate)
    : m_selection{selection}, m_scene{std::move(scene)},
      m_loader{std::move(loader)}, m_seed_coordinate{seed_coordinate} {}

void MapStreamingSystem::frame_begin(Timestep /*frame_time*/) { tick(); }

void MapStreamingSystem::tick() {
  auto completed = m_loader->take_completed();
  auto started = m_loader->take_started();

  if (!m_grid) {
    for (const auto &result : completed) {
      if (result.request.key ==
              MapLoadKey{m_seed_coordinate, MapLayer::Terrain} &&
          result.error.empty() && result.payload &&
          m_loader->is_current(result.request)) {
        m_grid = MapGridTransform::from_map(m_seed_coordinate,
                                             result.payload->map);
        if (m_grid) {
          m_selection.set_grid(*m_grid);
          m_scene->place_camera_for_seed(result.payload->map);
        }
        break;
      }
    }
  }

  if (m_grid) {
    m_selection.set_current(m_grid->coordinate_at(m_scene->camera_xy()));
  } else {
    m_selection.clear_current();
  }
  m_desired = desired_map_residency(m_selection.catalog(),
                                    m_selection.residency_intent());

  for (const auto coordinate : m_selection.take_retry_requests()) {
    for (const auto layer : {MapLayer::Terrain, MapLayer::Detail}) {
      const MapLoadKey key{coordinate, layer};
      if (m_failed.erase(key) > 0) {
        set_status(key, MapResidencyStatus::NotResident);
      }
    }
  }

  for (const auto &request : started) {
    if (m_loader->is_current(request) && is_desired(request.key)) {
      set_status(request.key, MapResidencyStatus::Loading);
    }
  }

  std::vector<MapLoadKey> cancellations;
  for (auto requested = m_requested_generations.begin();
       requested != m_requested_generations.end();) {
    if (!is_desired(requested->first)) {
      cancellations.push_back(requested->first);
      m_in_flight.erase(requested->first);
      set_status(requested->first, MapResidencyStatus::NotResident);
      requested = m_requested_generations.erase(requested);
    } else {
      ++requested;
    }
  }

  for (auto failed = m_failed.begin(); failed != m_failed.end();) {
    if (!is_desired(*failed)) {
      set_status(*failed, MapResidencyStatus::NotResident);
      failed = m_failed.erase(failed);
    } else {
      ++failed;
    }
  }

  for (auto group = m_groups.begin(); group != m_groups.end();) {
    if (!is_desired(group->first)) {
      m_scene->remove(group->second);
      set_status(group->first, MapResidencyStatus::NotResident);
      group = m_groups.erase(group);
    } else {
      ++group;
    }
  }

  for (auto &result : completed) {
    const auto generation = m_requested_generations.find(result.request.key);
    const auto owns_generation =
        generation != m_requested_generations.end() &&
        generation->second == result.request.generation;
    if (owns_generation) {
      m_in_flight.erase(result.request.key);
      m_requested_generations.erase(generation);
    }

    if (!m_loader->is_current(result.request) ||
        !is_desired(result.request.key) || !owns_generation) {
      continue;
    }

    if (!result.error.empty() || !result.payload) {
      m_failed.insert(result.request.key);
      set_status(result.request.key, MapResidencyStatus::Failed,
                 result.error.empty() ? "Map load returned no data"
                                      : result.error);
      continue;
    }

    if (result.request.key ==
            MapLoadKey{m_seed_coordinate, MapLayer::Terrain} &&
        !m_grid) {
      m_failed.insert(result.request.key);
      set_status(result.request.key, MapResidencyStatus::Failed,
                 "Seed terrain has an invalid bounding box");
      continue;
    }

    try {
      const auto group = m_scene->upload(result.request.key.coordinate,
                                         result.request.key.layer,
                                         *result.payload);
      m_groups.insert_or_assign(result.request.key, group);
      m_selection.set_bounds(result.request.key.coordinate,
                             result.payload->map.bounding_box);
      set_status(result.request.key, MapResidencyStatus::Resident);
    } catch (const std::exception &error) {
      m_failed.insert(result.request.key);
      set_status(result.request.key, MapResidencyStatus::Failed, error.what());
    }
  }

  // A resident Detail replaces only the same square's cheap terrain draw.
  // Keep the terrain group allocated as an immediate fallback on failure or
  // unloading; this also handles Detail arriving before Terrain.
  for (const auto &[key, group] : m_groups) {
    if (key.layer == MapLayer::Terrain) {
      m_scene->set_visible(group,
                           !m_groups.contains({key.coordinate,
                                               MapLayer::Detail}));
    }
  }

  std::vector<MapLoadRequest> requests;
  const auto enqueue_missing = [this, &requests](MapLoadKey key) {
    if (m_groups.contains(key) || m_failed.contains(key)) {
      return;
    }
    const auto requested = m_requested_generations.find(key);
    if (requested != m_requested_generations.end()) {
      requests.push_back(make_request(key, requested->second));
      return;
    }
    auto request = make_request(key);
    m_requested_generations.insert_or_assign(key, request.generation);
    m_in_flight.insert(key);
    set_status(key, MapResidencyStatus::Queued);
    requests.push_back(std::move(request));
  };

  for (const auto coordinate : m_desired.detail) {
    enqueue_missing({coordinate, MapLayer::Detail});
  }
  for (const auto coordinate : m_desired.terrain) {
    enqueue_missing({coordinate, MapLayer::Terrain});
  }

  m_loader->reconcile(std::move(requests), std::move(cancellations));
}

auto MapStreamingSystem::resident() const -> DesiredMapResidency {
  DesiredMapResidency result;
  for (const auto &[key, group] : m_groups) {
    (void)group;
    if (key.layer == MapLayer::Terrain) {
      result.terrain.insert(key.coordinate);
    } else {
      result.detail.insert(key.coordinate);
    }
  }
  return result;
}

auto MapStreamingSystem::is_desired(MapLoadKey key) const -> bool {
  return key.layer == MapLayer::Terrain
             ? m_desired.terrain.contains(key.coordinate)
             : m_desired.detail.contains(key.coordinate);
}

auto MapStreamingSystem::make_request(MapLoadKey key,
                                      std::optional<std::uint64_t> generation)
    -> MapLoadRequest {
  const auto *region = m_selection.catalog().find(key.coordinate);
  const auto priority = [&] {
    if (key.layer == MapLayer::Detail) {
      return m_selection.current() == key.coordinate
                 ? MapLoadPriority::CurrentDetail
                 : MapLoadPriority::NeighborDetail;
    }
    return m_desired.detail.contains(key.coordinate)
               ? MapLoadPriority::AutomaticTerrain
               : MapLoadPriority::ManualTerrain;
  }();
  const auto request_generation =
      generation ? *generation : m_next_generation++;
  return {key, *region, request_generation, priority};
}

void MapStreamingSystem::set_status(MapLoadKey key, MapResidencyStatus status,
                                    std::string error) {
  m_selection.set_status(key.coordinate, key.layer, status, error);
  utils::Log(status == MapResidencyStatus::Failed ? utils::LOG_WARN
                                                  : utils::LOG_INFO,
             "App")
      << "Map residency map=" << key.coordinate.x << '_' << key.coordinate.y
      << " layer=" << layer_name(key.layer) << " state=" << status_name(status)
      << (error.empty() ? std::string{} : " error=" + error) << std::endl;
}

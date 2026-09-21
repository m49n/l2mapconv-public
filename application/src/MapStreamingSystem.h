#pragma once

#include "MapGridTransform.h"
#include "MapLoadService.h"
#include "MapResidency.h"
#include "MapSceneSink.h"
#include "MapSelectionContext.h"
#include "System.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>

class MapStreamingSystem final : public System {
public:
  MapStreamingSystem(MapSelectionContext &selection,
                     std::unique_ptr<MapLoadService> loader,
                     std::unique_ptr<MapSceneSink> scene,
                     MapCoordinate seed_coordinate);

  void frame_begin(Timestep frame_time) override;
  void tick();
  auto resident() const -> DesiredMapResidency;

private:
  MapSelectionContext &m_selection;
  std::unique_ptr<MapSceneSink> m_scene;
  std::unique_ptr<MapLoadService> m_loader;
  MapCoordinate m_seed_coordinate;
  std::optional<MapGridTransform> m_grid;
  DesiredMapResidency m_desired;
  std::map<MapLoadKey, rendering::SceneGroupId> m_groups;
  std::map<MapLoadKey, std::uint64_t> m_requested_generations;
  std::set<MapLoadKey> m_in_flight;
  std::set<MapLoadKey> m_failed;
  std::uint64_t m_next_generation{1};

  auto is_desired(MapLoadKey key) const -> bool;
  auto make_request(MapLoadKey key,
                    std::optional<std::uint64_t> generation = std::nullopt)
      -> MapLoadRequest;
  void set_status(MapLoadKey key, MapResidencyStatus status,
                  std::string error = {});
};

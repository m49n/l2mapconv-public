#pragma once

#include "Map.h"
#include "MapCatalog.h"
#include "MapLoadOptions.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

enum class MapLoadPriority : std::uint8_t {
  CurrentDetail = 0,
  NeighborDetail = 1,
  AutomaticTerrain = 2,
  ManualTerrain = 3,
};

struct MapLoadKey {
  MapCoordinate coordinate;
  MapLayer layer;
  auto operator<=>(const MapLoadKey &) const = default;
};

struct MapLoadRequest {
  MapLoadKey key;
  MapRegion region;
  std::uint64_t generation;
  MapLoadPriority priority;
};

struct MapLoadResult {
  MapLoadRequest request;
  std::optional<Map> map;
  std::string error;
};

class MapLoadQueue {
public:
  void enqueue(MapLoadRequest request);
  void cancel(MapLoadKey key);
  void clear_pending();

  auto pop() -> std::optional<MapLoadRequest>;
  auto is_current(const MapLoadRequest &request) const -> bool;
  auto empty() const -> bool;
  auto size() const -> std::size_t;

private:
  std::map<MapLoadKey, MapLoadRequest> m_latest;
  std::vector<MapLoadRequest> m_pending;
};

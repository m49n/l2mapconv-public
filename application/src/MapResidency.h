#pragma once

#include "MapCatalog.h"

#include <optional>
#include <set>

using Coordinates = std::set<MapCoordinate>;

struct MapResidencyIntent {
  Coordinates manual;
  std::optional<MapCoordinate> current;
  bool auto_load{true};
  bool include_neighbors{true};
};

struct DesiredMapResidency {
  Coordinates terrain;
  Coordinates detail;
};

auto desired_map_residency(const MapCatalog &catalog,
                           const MapResidencyIntent &intent)
    -> DesiredMapResidency;

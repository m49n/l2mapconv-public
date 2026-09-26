#pragma once

#include <compare>

enum class MapLayer { Terrain, Detail };

struct MapLoadOptions {
  bool terrain;
  bool static_meshes;
  bool csg;
  bool blocking_volumes;
  auto operator<=>(const MapLoadOptions &) const = default;

  static constexpr auto terrain_only() -> MapLoadOptions {
    return {true, false, false, false};
  }

  static constexpr auto detail_only() -> MapLoadOptions {
    return {false, true, true, true};
  }

  static constexpr auto blocking_only() -> MapLoadOptions {
    return {false, false, false, true};
  }

  static constexpr auto full() -> MapLoadOptions {
    return {true, true, true, true};
  }
};

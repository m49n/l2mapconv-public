#pragma once

#include "Entity.h"

#include <cstdint>

struct SurfaceVisibility {
  bool passable{};
  bool terrain{};
  bool static_meshes{};
  bool csg{};
  bool blocking_volumes{};
  bool bounding_boxes{};
  bool imported_geodata{};
  bool generated_geodata{};
};

inline auto surface_filter(const SurfaceVisibility &visibility)
    -> std::uint64_t {
  auto filter = std::uint64_t{};

  if (visibility.passable) {
    filter |= SURFACE_PASSABLE;
  }
  if (visibility.terrain) {
    filter |= SURFACE_TERRAIN;
  }
  if (visibility.static_meshes) {
    filter |= SURFACE_STATIC_MESH;
  }
  if (visibility.csg) {
    filter |= SURFACE_CSG;
  }
  if (visibility.blocking_volumes) {
    filter |= SURFACE_BLOCKING_VOLUME;
  }
  if (visibility.bounding_boxes) {
    filter |= SURFACE_BOUNDING_BOX;
  }
  if (visibility.imported_geodata) {
    filter |= SURFACE_IMPORTED_GEODATA;
  }
  if (visibility.generated_geodata) {
    filter |= SURFACE_GENERATED_GEODATA;
  }

  return filter;
}

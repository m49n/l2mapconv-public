#include "MapNavigation.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

auto map_focus_position(glm::vec2 center, std::optional<geometry::Box> bounds,
                        float current_z) -> glm::vec3 {
  if (!std::isfinite(center.x) || !std::isfinite(center.y))
    throw std::invalid_argument{"Invalid map center"};
  float z = std::isfinite(current_z) ? std::max(current_z, 32768.0f) : 32768.0f;
  if (bounds) {
    const auto lo = bounds->min(), hi = bounds->max();
    bool valid = true;
    for (int axis = 0; axis < 3; ++axis)
      valid = valid && std::isfinite(lo[axis]) && std::isfinite(hi[axis]) &&
              lo[axis] <= hi[axis];
    if (valid && std::isfinite(hi.z + 8192.0f))
      z = hi.z + 8192.0f;
  }
  return {center, z};
}
auto map_focus_target(const MapSelectionContext &selection,
                      MapCoordinate clicked, float current_z)
    -> std::optional<glm::vec3> {
  if (!selection.grid() || !selection.catalog().contains(clicked))
    return std::nullopt;
  return map_focus_position(selection.grid()->world_center(clicked),
                            selection.bounds(clicked), current_z);
}

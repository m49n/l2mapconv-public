#pragma once

#include "Map.h"
#include "MapCatalog.h"

#include <glm/glm.hpp>

#include <optional>

class MapGridTransform {
public:
  MapGridTransform(MapCoordinate anchor, glm::vec2 world_minimum,
                   glm::vec2 tile_size);

  static auto from_map(MapCoordinate anchor, const Map &map)
      -> std::optional<MapGridTransform>;

  auto coordinate_at(glm::vec2 world_position) const -> MapCoordinate;

private:
  MapCoordinate m_anchor;
  glm::vec2 m_world_minimum;
  glm::vec2 m_tile_size;
};

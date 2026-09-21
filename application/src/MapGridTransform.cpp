#include "MapGridTransform.h"

#include <cmath>
#include <stdexcept>

MapGridTransform::MapGridTransform(MapCoordinate anchor,
                                   glm::vec2 world_minimum,
                                   glm::vec2 tile_size)
    : m_anchor{anchor}, m_world_minimum{world_minimum}, m_tile_size{tile_size} {
  if (tile_size.x <= 0.0f || tile_size.y <= 0.0f) {
    throw std::invalid_argument{"Map grid tile size must be positive"};
  }
}

auto MapGridTransform::from_map(MapCoordinate anchor, const Map &map)
    -> std::optional<MapGridTransform> {
  const auto minimum = map.bounding_box.min();
  const auto maximum = map.bounding_box.max();
  const glm::vec2 tile_size{maximum.x - minimum.x, maximum.y - minimum.y};
  if (tile_size.x <= 0.0f || tile_size.y <= 0.0f) {
    return std::nullopt;
  }

  return MapGridTransform{anchor, {minimum.x, minimum.y}, tile_size};
}

auto MapGridTransform::coordinate_at(glm::vec2 world_position) const
    -> MapCoordinate {
  const auto offset = (world_position - m_world_minimum) / m_tile_size;
  return {m_anchor.x + static_cast<int>(std::floor(offset.x)),
          m_anchor.y + static_cast<int>(std::floor(offset.y))};
}

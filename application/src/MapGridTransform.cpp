#include "MapGridTransform.h"

#include <cmath>
#include <stdexcept>

auto MapGridTransform::world_center(MapCoordinate coordinate) const -> glm::vec2 {
  return m_world_minimum + m_tile_size * glm::vec2{
      static_cast<float>(coordinate.x) - static_cast<float>(m_anchor.x) + 0.5f,
      static_cast<float>(coordinate.y) - static_cast<float>(m_anchor.y) + 0.5f};
}

MapGridTransform::MapGridTransform(MapCoordinate anchor,
                                   glm::vec2 world_minimum,
                                   glm::vec2 tile_size)
    : m_anchor{anchor}, m_world_minimum{world_minimum}, m_tile_size{tile_size} {
  if (!std::isfinite(world_minimum.x) || !std::isfinite(world_minimum.y) ||
      !std::isfinite(tile_size.x) || !std::isfinite(tile_size.y) ||
      tile_size.x <= 0.0f || tile_size.y <= 0.0f) {
    throw std::invalid_argument{"Map grid tile size must be positive"};
  }
}

auto MapGridTransform::from_map(MapCoordinate anchor, const Map &map)
    -> std::optional<MapGridTransform> {
  const auto minimum = map.bounding_box.min();
  const auto maximum = map.bounding_box.max();
  const glm::vec2 tile_size{maximum.x - minimum.x, maximum.y - minimum.y};
  if (!std::isfinite(minimum.x) || !std::isfinite(minimum.y) ||
      !std::isfinite(tile_size.x) || !std::isfinite(tile_size.y) ||
      tile_size.x <= 0.0f || tile_size.y <= 0.0f) {
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

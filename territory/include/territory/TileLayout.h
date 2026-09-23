#pragma once
#include "VisualScene.h"
namespace territory {
struct Tile {
  int x{}, y{}, width{}, height{};
  Bounds world;
};
std::vector<Tile> tile_layout(const Bounds &, int pixels, int tile_size);
glm::dvec2 world_to_pixel(const Bounds &, int pixels, glm::dvec2 world);
} // namespace territory

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <territory/TileLayout.h>
namespace territory {
namespace {
void valid(const Bounds &b, int n) {
  if (n < 128 || n > 16384)
    throw std::invalid_argument("Raster size must be between 128 and 16384");
  for (double v : {b.min_x, b.min_y, b.max_x, b.max_y, b.min_z, b.max_z})
    if (!std::isfinite(v))
      throw std::invalid_argument("Non-finite raster bounds");
  if (!(b.max_x > b.min_x && b.max_y > b.min_y && b.max_z >= b.min_z))
    throw std::invalid_argument("Invalid raster bounds");
  if (!std::isfinite(b.max_x - b.min_x) || !std::isfinite(b.max_y - b.min_y) ||
      !std::isfinite(b.max_z - b.min_z))
    throw std::invalid_argument("Raster extent overflow");
}
} // namespace
std::vector<Tile> tile_layout(const Bounds &b, int n, int size) {
  valid(b, n);
  if (size < 1 || size > 2048)
    throw std::invalid_argument("Tile size must be between 1 and 2048");
  // A tiny tile at 16K could allocate hundreds of millions of entries.
  const auto side = (static_cast<std::size_t>(n) + size - 1) / size;
  if (side * side > 16384)
    throw std::invalid_argument("Too many raster tiles");
  std::vector<Tile> result;
  result.reserve(side * side);
  for (int y = 0; y < n; y += size)
    for (int x = 0; x < n; x += size) {
      const int w = std::min(size, n - x), h = std::min(size, n - y);
      auto z0 = b.min_z, z1 = b.max_z;
      if (z0 == z1) {
        z0 -= 128;
        z1 += 128;
      }
      result.push_back({x,
                        y,
                        w,
                        h,
                        {b.min_x + (b.max_x - b.min_x) * x / n,
                         b.min_y + (b.max_y - b.min_y) * y / n,
                         b.min_x + (b.max_x - b.min_x) * (x + w) / n,
                         b.min_y + (b.max_y - b.min_y) * (y + h) / n, z0, z1}});
    }
  return result;
}
glm::dvec2 world_to_pixel(const Bounds &b, int n, glm::dvec2 p) {
  valid(b, n);
  if (!std::isfinite(p.x) || !std::isfinite(p.y))
    throw std::invalid_argument("Non-finite world point");
  return {(p.x - b.min_x) * n / (b.max_x - b.min_x),
          (p.y - b.min_y) * n / (b.max_y - b.min_y)};
}
} // namespace territory

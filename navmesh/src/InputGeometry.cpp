#include <algorithm>
#include <climits>
#include <cmath>
#include <navmesh/InputGeometry.h>
#include <navmesh/Navmesh.h>
#include <stdexcept>
namespace navmesh {
namespace {
void check_cancel(const std::function<bool()> &cancel) {
  if (cancel && cancel())
    throw Cancelled{};
}
void check_size(std::size_t vertices, std::size_t indices) {
  if (vertices > INT_MAX || indices > INT_MAX || indices % 3)
    throw std::runtime_error("Navmesh context exceeds indexed geometry limits");
}
} // namespace
InputGeometry input_geometry(const geodata::Map &map,
                             const std::function<bool()> &cancel) {
  check_size(map.vertices().size(), map.indices().size());
  InputGeometry out{map.bounding_box()};
  // Bounded chunks allow cancellation even for a large own-map copy.
  auto copy = [&](const auto &source, auto &target) {
    check_cancel(cancel);
    target.reserve(source.size());
    for (std::size_t i = 0; i < source.size(); i += 65536) {
      check_cancel(cancel);
      const auto end = std::min(source.size(), i + 65536);
      target.insert(target.end(), source.begin() + i, source.begin() + end);
    }
  };
  copy(map.vertices(), out.vertices);
  copy(map.indices(), out.indices);
  return out;
}
double context_padding(const Settings &settings) {
  validate(settings);
  return (std::ceil(double(settings.actor_radius) / settings.cell_size) + 3) *
         settings.cell_size;
}
void validate_region_grid(const Settings &settings) {
  validate(settings);
  const double tiles =
      32768.0 / (double(settings.cell_size) * settings.tile_cells);
  if (tiles != std::floor(tiles))
    throw std::runtime_error(
        "Navmesh neighbor context requires tile width to divide 32768 exactly");
}
void append_context(InputGeometry &out, const geodata::Map &map, double padding,
                    const std::function<bool()> &cancel) {
  check_cancel(cancel);
  if (!std::isfinite(padding) || padding < 0)
    throw std::runtime_error("Invalid navmesh context padding");
  const auto &v = map.vertices();
  const auto &indices = map.indices();
  check_size(v.size(), indices.size());
  check_size(out.vertices.size(), out.indices.size());
  const auto lo = out.game_bounds.min(), hi = out.game_bounds.max();
  std::vector<unsigned> remap(v.size(), UINT_MAX);
  for (std::size_t i = 0; i < indices.size(); i += 3) {
    if ((i & 4095) == 0)
      check_cancel(cancel);
    for (int k = 0; k < 3; ++k)
      if (indices[i + k] >= v.size())
        throw std::runtime_error("Navmesh context index out of range");
    const auto a = v[indices[i]], b = v[indices[i + 1]], c = v[indices[i + 2]];
    for (const auto p : {a, b, c})
      if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
        throw std::runtime_error("Nonfinite navmesh context vertex");
    // Conservative triangle AABB overlap also keeps crossing walls whose
    // vertices and actor origin all lie outside the requested strip.
    if (std::max({a.x, b.x, c.x}) < lo.x - padding ||
        std::min({a.x, b.x, c.x}) > hi.x + padding ||
        std::max({a.z, b.z, c.z}) < lo.y - padding ||
        std::min({a.z, b.z, c.z}) > hi.y + padding)
      continue;
    check_size(out.vertices.size() + 3, out.indices.size() + 3);
    for (int k = 0; k < 3; ++k) {
      auto &mapped = remap[indices[i + k]];
      if (mapped == UINT_MAX) {
        mapped = static_cast<unsigned>(out.vertices.size());
        out.vertices.push_back(v[indices[i + k]]);
      }
      out.indices.push_back(mapped);
    }
  }
}
} // namespace navmesh

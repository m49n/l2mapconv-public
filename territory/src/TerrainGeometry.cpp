#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <territory/TerrainGeometry.h>

namespace territory {
namespace {
glm::vec3 vec(unreal::Vector v) { return {v.x, v.y, v.z}; }
bool finite(glm::vec3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
struct Heightfield {
  std::shared_ptr<unreal::TerrainInfoActor> actor;
  std::shared_ptr<unreal::Texture> texture;
  glm::vec3 origin, scale;
  int width, height;

  explicit Heightfield(const std::shared_ptr<unreal::TerrainInfoActor> &t)
      : actor(t) {
    if (!t || t->broken_scale() || !t->terrain_map.has_reference())
      throw std::runtime_error("Invalid visual terrain");
    texture = t->terrain_map.as<unreal::Texture>();
    if (!texture || texture->format != unreal::TEXF_G16 ||
        texture->mips.empty())
      throw std::runtime_error("Missing G16 visual heightmap");
    width = texture->u_size;
    height = texture->v_size;
    if (width < 2 || height < 2 || width > 4096 || height > 4096 ||
        texture->mips[0].data.size() != std::size_t(width) * height * 2)
      throw std::runtime_error("Invalid terrain height dimensions");
    origin = vec(t->position());
    scale = vec(t->scale());
    if (!finite(origin) || !finite(scale) || scale.x <= 0 || scale.y <= 0 ||
        std::abs(width * scale.x - 32768) > .01 ||
        std::abs(height * scale.y - 32768) > .01)
      throw std::runtime_error(
          "Invalid or unsupported terrain extent/transform");
    if (t->edge_turn_bitmap.size() < std::size_t(width) * height)
      throw std::runtime_error("Incomplete terrain diagonal bitmap");
  }

  double raw(int x, int y) const {
    const auto i = std::size_t(y * width + x) * 2;
    const auto &bytes = texture->mips[0].data;
    return bytes[i] | (unsigned(bytes[i + 1]) << 8);
  }
  glm::dvec2 local(double x, double y) const {
    return {(x - origin.x) / scale.x, (y - origin.y) / scale.y};
  }
  bool covers(double x, double y) const {
    const auto p = local(x, y);
    return p.x >= 0 && p.y >= 0 && p.x <= width - 1 && p.y <= height - 1;
  }
  float sample(double x, double y) const {
    const auto p = local(x, y);
    const double gx = std::clamp(p.x, 0., double(width - 1));
    const double gy = std::clamp(p.y, 0., double(height - 1));
    const int ix = std::min(int(std::floor(gx)), width - 2);
    const int iy = std::min(int(std::floor(gy)), height - 2);
    const double fx = gx - ix, fy = gy - iy;
    const auto a = raw(ix, iy), b = raw(ix + 1, iy);
    const auto c = raw(ix, iy + 1), d = raw(ix + 1, iy + 1);
    double z;
    // Interpolate the actual heightfield triangle, not a bilinear surface:
    // this is the same diagonal convention used for the owned geometry below.
    if (actor->edge_turn_bitmap[iy * width + ix])
      z = fy <= fx ? a + (b - a) * fx + (d - b) * fy
                   : a + (d - c) * fx + (c - a) * fy;
    else
      z = fx + fy <= 1 ? a + (b - a) * fx + (c - a) * fy
                       : d + (c - d) * (1 - fx) + (b - d) * (1 - fy);
    return static_cast<float>(z * scale.z + origin.z);
  }
};
} // namespace

TerrainGeometry build_terrain_mesh(
    const std::array<std::shared_ptr<unreal::TerrainInfoActor>, 4> &edges,
    const Bounds &crop, const Cancel &cancel) {
  check_cancel(cancel);
  const auto terrain = edges[0];
  Heightfield owner(terrain);
  const auto origin = owner.origin, scale = owner.scale;
  const auto w = owner.width, h = owner.height;
  // Actor transform positions geometry; filename positions the radar crop.
  if (!std::isfinite(crop.min_x) || !std::isfinite(crop.min_y) ||
      !std::isfinite(crop.max_x) || !std::isfinite(crop.max_y) ||
      crop.min_x >= crop.max_x || crop.min_y >= crop.max_y ||
      origin.x >= crop.max_x || origin.y >= crop.max_y ||
      origin.x + w * scale.x <= crop.min_x ||
      origin.y + h * scale.y <= crop.min_y)
    throw std::runtime_error("Terrain does not overlap requested square");

  std::array<std::optional<Heightfield>, 4> fields;
  fields[0] = owner;
  for (int i = 1; i < 4; ++i) {
    check_cancel(cancel);
    if (edges[i] && !edges[i]->broken_scale() &&
        edges[i]->terrain_map.has_reference()) {
      fields[i].emplace(edges[i]);
      if (fields[i]->width != w || fields[i]->height != h)
        throw std::runtime_error("Invalid adjacent terrain dimensions");
    }
  }
  TerrainGeometry result;
  auto &grid = result.mesh;
  grid.vertices.resize(std::size_t(w + 1) * (h + 1));
  for (int y = 0; y <= h; ++y) {
    check_cancel(cancel);
    for (int x = 0; x <= w; ++x) {
      auto &v = grid.vertices[y * (w + 1) + x];
      v.position = {origin.x + x * scale.x, origin.y + y * scale.y, 0};
      if (x < w && y < h) {
        // Keep original sample arithmetic for byte-stable aligned terrain.
        v.position.z = static_cast<float>(owner.raw(x, y)) * scale.z + origin.z;
      } else {
        const int preferred = (x == w ? 1 : 0) + (y == h ? 2 : 0);
        const Heightfield *source = nullptr;
        if (fields[preferred] &&
            fields[preferred]->covers(v.position.x, v.position.y))
          source = &*fields[preferred];
        else
          for (const auto &field : fields)
            if (field && field->covers(v.position.x, v.position.y)) {
              source = &*field;
              break;
            }
        if (!source) {
          // No evidence at this XY. Clamp the preferred neighbour (or owner
          // if absent) and explicitly report the approximation to the caller.
          source = fields[preferred] ? &*fields[preferred] : &owner;
          ++result.clamped_border_samples;
        }
        v.position.z = source->sample(v.position.x, v.position.y);
      }
      if (!finite(v.position))
        throw std::runtime_error("Non-finite terrain vertex");
      v.uv[0] = {float(x) / w, float(y) / h};
    }
  }
  if (terrain->quad_visibility_bitmap.size() <
          static_cast<std::size_t>(w) * h ||
      terrain->edge_turn_bitmap.size() < static_cast<std::size_t>(w) * h)
    throw std::runtime_error("Incomplete terrain visibility/diagonal bitmap");
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      auto bit = y * w + x;
      if (!terrain->quad_visibility_bitmap[bit])
        continue;
      auto a = std::uint32_t(y * (w + 1) + x), b = a + 1, c = a + w + 1,
           d = c + 1;
      if (terrain->edge_turn_bitmap[bit])
        grid.indices.insert(grid.indices.end(), {a, b, d, a, d, c});
      else
        grid.indices.insert(grid.indices.end(), {a, b, c, b, d, c});
    }
  for (std::size_t i = 0; i < grid.indices.size(); i += 3) {
    auto a = grid.indices[i], b = grid.indices[i + 1], c = grid.indices[i + 2];
    auto n = glm::cross(grid.vertices[b].position - grid.vertices[a].position,
                        grid.vertices[c].position - grid.vertices[a].position);
    for (auto j : {a, b, c})
      grid.vertices[j].normal += n;
  }
  for (auto &v : grid.vertices)
    v.normal = glm::length(v.normal) > 0 ? glm::normalize(v.normal)
                                         : glm::vec3(0, 0, 1);

  return result;
}
} // namespace territory

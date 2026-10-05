#include "BuildLimits.h"
#include <DetourNavMeshBuilder.h>
#include <Recast.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
#include <navmesh/Navmesh.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace navmesh {
void detail::validate_detail_limits(std::span<const unsigned short> polygons,
                                    int nvp,
                                    std::span<const unsigned int> meshes) {
  if (nvp < 3 || nvp > 6 || meshes.size() % 4 ||
      polygons.size() != meshes.size() / 4 * nvp * 2)
    throw std::runtime_error("Invalid navmesh detail mesh layout");
  std::size_t total = 0;
  for (std::size_t i = 0; i < meshes.size() / 4; ++i) {
    unsigned base = 0;
    while (base < unsigned(nvp) &&
           polygons[i * nvp * 2 + base] != RC_MESH_NULL_IDX)
      ++base;
    const auto count = meshes[i * 4 + 1];
    if (base < 3 || count < base || count - base > 255 ||
        meshes[i * 4 + 3] > 255)
      throw std::runtime_error(
          "Navmesh detail mesh exceeds per-polygon limits");
    total += count - base;
    if (total > 65535)
      throw std::runtime_error("Navmesh tile exceeds Detour detail vertex "
                               "offsets; reduce tile size");
  }
}
namespace {
constexpr std::size_t max_tiles = 65536, max_bin_references = 100000000;
void require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void check_cancel(const Cancel &cancel) {
  if (cancel && cancel())
    throw Cancelled{};
}
class Context : public rcContext {
public:
  std::string error;
  void check(bool ok, const char *stage) {
    if (!ok || !error.empty())
      throw std::runtime_error(std::string("Navmesh ") + stage + ": " + error);
  }

protected:
  void doLog(rcLogCategory category, const char *message, int length) override {
    if (category == RC_LOG_ERROR) {
      if (!error.empty())
        error += "; ";
      error.append(message, length);
    }
  }
};
bool finite(glm::vec3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
void validate_spans(const rcHeightfield &hf) {
  std::size_t total = 0;
  for (int i = 0; i < hf.width * hf.height; ++i) {
    int count = 0;
    for (auto *s = hf.spans[i]; s; s = s->next)
      if (s->area != RC_NULL_AREA)
        ++count;
    require(count <= 63,
            "Navmesh exceeds compact heightfield layer connections");
    total += count;
  }
  require(total < (1U << 24), "Navmesh exceeds compact heightfield span index");
}
} // namespace

void validate(const Settings &s) {
  for (float v : {s.actor_height, s.actor_radius, s.max_climb, s.max_slope,
                  s.cell_size, s.cell_height})
    require(std::isfinite(v), "Navmesh settings must be finite");
  require(s.cell_size >= 1 && s.cell_size <= 64 && s.cell_height >= .25f &&
              s.cell_height <= 16,
          "Navmesh voxel size is outside supported limits");
  require(s.actor_height > 0 && s.actor_radius >= 0 && s.max_climb >= 0 &&
              s.max_slope >= 0 && s.max_slope < 90,
          "Invalid navmesh actor settings");
  require(std::ceil(s.actor_height / s.cell_height) <= 255 &&
              s.max_climb / s.cell_height <= 255 &&
              s.actor_radius / s.cell_size <= 64,
          "Navmesh actor dimensions exceed compact heightfield limits");
  require(s.tile_cells >= 8 && s.tile_cells <= 256 &&
              (s.tile_cells & (s.tile_cells - 1)) == 0,
          "Navmesh tile size must be a power of two from 8 to 256");
}

Result build(const geodata::Map &map, const Settings &s, const Cancel &cancel,
             const Progress &progress) {
  return build(input_geometry(map, cancel), s, cancel, progress);
}
Result build(const InputGeometry &input, const Settings &s, const Cancel &cancel,
             const Progress &progress) {
  validate(s);
  check_cancel(cancel);
  const auto &vertices = input.vertices;
  const auto &indices = input.indices;
  require(!vertices.empty() && !indices.empty() && indices.size() % 3 == 0 &&
              vertices.size() <= INT_MAX && indices.size() <= INT_MAX,
          "Navmesh requires a nonempty indexed triangle mesh within integer "
          "limits");
  const auto game_lo = input.game_bounds.min(), game_hi = input.game_bounds.max();
  const glm::vec3 lo{game_lo.x, game_lo.z, game_lo.y};
  const glm::vec3 hi{game_hi.x, game_hi.z, game_hi.y};
  require(finite(lo) && finite(hi) && hi.x > lo.x && hi.z > lo.z,
          "Invalid navmesh horizontal bounds");
  const double tile_width = double(s.cell_size) * s.tile_cells;
  require(std::max({std::abs(lo.x), std::abs(lo.z), std::abs(hi.x),
                    std::abs(hi.z)}) < 10000000,
          "Navmesh world bounds exceed supported coordinate range");
  const int x0 = static_cast<int>(std::floor(lo.x / tile_width));
  const int y0 = static_cast<int>(std::floor(lo.z / tile_width));
  const int x1 = static_cast<int>(std::ceil(hi.x / tile_width));
  const int y1 = static_cast<int>(std::ceil(hi.z / tile_width));
  const auto nx = x1 - x0, ny = y1 - y0;
  const auto count = std::size_t(nx) * ny;
  require(nx > 0 && ny > 0 && count <= max_tiles,
          "Navmesh tile grid is too large");
  float min_height = std::numeric_limits<float>::max(),
        max_height = -min_height;
  std::vector<float> verts;
  verts.reserve(vertices.size() * 3);
  std::size_t copied = 0;
  for (const auto &v : vertices) {
    if ((copied++ & 4095) == 0) check_cancel(cancel);
    require(finite(v) && std::max({std::abs(v.x), std::abs(v.y),
                                   std::abs(v.z)}) < 10000000,
            "Navmesh has nonfinite or out-of-range vertices");
    min_height = std::min(min_height, v.y);
    max_height = std::max(max_height, v.y);
    verts.insert(verts.end(), {v.x, v.y, v.z});
  }
  min_height =
      std::floor(min_height / s.cell_height) * s.cell_height - s.cell_height;
  max_height =
      std::ceil(max_height / s.cell_height) * s.cell_height + s.cell_height;
  require((double(max_height) - min_height) / s.cell_height < 65534,
          "Navmesh vertical range exceeds 16-bit heightfield; increase cell "
          "height");

  rcConfig cfg{};
  cfg.cs = s.cell_size;
  cfg.ch = s.cell_height;
  cfg.walkableSlopeAngle = s.max_slope;
  cfg.walkableHeight =
      static_cast<int>(std::ceil(s.actor_height / s.cell_height));
  cfg.walkableRadius =
      static_cast<int>(std::ceil(s.actor_radius / s.cell_size));
  cfg.walkableClimb = static_cast<int>(std::floor(s.max_climb / s.cell_height));
  cfg.tileSize = s.tile_cells;
  cfg.borderSize = cfg.walkableRadius + 3;
  cfg.width = cfg.height = cfg.tileSize + cfg.borderSize * 2;
  cfg.maxEdgeLen = 12;
  cfg.maxSimplificationError = 1.3f;
  // Preserve small legitimate islands; connectivity is determined by geometry.
  cfg.minRegionArea = 0;
  cfg.mergeRegionArea = 400;
  cfg.maxVertsPerPoly = 6;
  cfg.detailSampleDist = 6 * s.cell_size;
  cfg.detailSampleMaxError = s.cell_height;
  const double padding = cfg.borderSize * double(s.cell_size);
  std::vector<std::vector<int>> bins(count);
  std::size_t references = 0;
  for (std::size_t i = 0; i < indices.size(); i += 3) {
    if ((i & 4095) == 0)
      check_cancel(cancel);
    for (int j = 0; j < 3; ++j)
      require(indices[i + j] < vertices.size(),
              "Navmesh triangle index out of range");
    auto a = vertices[indices[i]], b = vertices[indices[i + 1]],
         c = vertices[indices[i + 2]];
    const double left = std::min({a.x, b.x, c.x}) - padding,
                 right = std::max({a.x, b.x, c.x}) + padding;
    const double bottom = std::min({a.z, b.z, c.z}) - padding,
                 top = std::max({a.z, b.z, c.z}) + padding;
    int tx0 = std::max(x0, static_cast<int>(std::floor(left / tile_width)));
    int tx1 =
        std::min(x1 - 1, static_cast<int>(std::floor(right / tile_width)));
    int ty0 = std::max(y0, static_cast<int>(std::floor(bottom / tile_width)));
    int ty1 = std::min(y1 - 1, static_cast<int>(std::floor(top / tile_width)));
    for (int y = ty0; y <= ty1; ++y)
      for (int x = tx0; x <= tx1; ++x) {
        require(++references <= max_bin_references,
                "Navmesh triangle bins exceed memory budget");
        auto &bin = bins[(y - y0) * nx + x - x0];
        for (int j = 0; j < 3; ++j)
          bin.push_back(static_cast<int>(indices[i + j]));
      }
  }
  Result result{Mesh{dtAllocNavMesh()}};
  require(bool(result.mesh), "Cannot allocate Detour mesh");
  dtNavMeshParams params{};
  params.tileWidth = params.tileHeight = static_cast<float>(tile_width);
  params.maxTiles = static_cast<int>(count);
  params.maxPolys = 1 << 20;
  require(dtStatusSucceed(result.mesh->init(&params)),
          "Cannot initialize Detour mesh");
  if (progress)
    progress(0, count);
  for (std::size_t tile = 0; tile < count; ++tile) {
    check_cancel(cancel);
    const auto &tris = bins[tile];
    if (tris.empty()) {
      ++result.empty_tiles;
      if (progress)
        progress(tile + 1, count);
      continue;
    }
    const int tx = x0 + static_cast<int>(tile % nx),
              ty = y0 + static_cast<int>(tile / nx);
    cfg.bmin[0] = static_cast<float>(tx * tile_width - padding);
    cfg.bmin[1] = min_height;
    cfg.bmin[2] = static_cast<float>(ty * tile_width - padding);
    cfg.bmax[0] = static_cast<float>((tx + 1) * tile_width + padding);
    cfg.bmax[1] = max_height;
    cfg.bmax[2] = static_cast<float>((ty + 1) * tile_width + padding);
    Context ctx;
    std::unique_ptr<rcHeightfield, decltype(&rcFreeHeightField)> hf(
        rcAllocHeightfield(), rcFreeHeightField);
    require(bool(hf), "Cannot allocate navmesh heightfield");
    ctx.check(rcCreateHeightfield(&ctx, *hf, cfg.width, cfg.height, cfg.bmin,
                                  cfg.bmax, cfg.cs, cfg.ch),
              "heightfield");
    std::vector<unsigned char> areas(tris.size() / 3, RC_NULL_AREA);
    rcMarkWalkableTriangles(&ctx, cfg.walkableSlopeAngle, verts.data(),
                            static_cast<int>(vertices.size()), tris.data(),
                            static_cast<int>(areas.size()), areas.data());
    ctx.check(rcRasterizeTriangles(
                  &ctx, verts.data(), static_cast<int>(vertices.size()),
                  tris.data(), areas.data(), static_cast<int>(areas.size()),
                  *hf, nullptr, cfg.walkableClimb),
              "rasterization");
    check_cancel(cancel);
    rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
    rcFilterLedgeSpans(&ctx, cfg.walkableHeight, cfg.walkableClimb, *hf);
    rcFilterWalkableLowHeightSpans(&ctx, cfg.walkableHeight, *hf);
    validate_spans(*hf);
    std::unique_ptr<rcCompactHeightfield, decltype(&rcFreeCompactHeightfield)>
        chf(rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
    require(bool(chf), "Cannot allocate compact heightfield");
    ctx.check(rcBuildCompactHeightfield(&ctx, cfg.walkableHeight,
                                        cfg.walkableClimb, *hf, *chf),
              "compact heightfield");
    hf.reset();
    ctx.check(rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf), "erosion");
    ctx.check(rcBuildDistanceField(&ctx, *chf), "distance field");
    ctx.check(rcBuildRegions(&ctx, *chf, cfg.borderSize, cfg.minRegionArea,
                             cfg.mergeRegionArea),
              "regions");
    check_cancel(cancel);
    std::unique_ptr<rcContourSet, decltype(&rcFreeContourSet)> contours(
        rcAllocContourSet(), rcFreeContourSet);
    std::unique_ptr<rcPolyMesh, decltype(&rcFreePolyMesh)> poly(
        rcAllocPolyMesh(), rcFreePolyMesh);
    std::unique_ptr<rcPolyMeshDetail, decltype(&rcFreePolyMeshDetail)> detail(
        rcAllocPolyMeshDetail(), rcFreePolyMeshDetail);
    require(contours && poly && detail,
            "Cannot allocate navmesh contours or polygons");
    ctx.check(rcBuildContours(&ctx, *chf, cfg.maxSimplificationError,
                              cfg.maxEdgeLen, *contours),
              "contours");
    ctx.check(rcBuildPolyMesh(&ctx, *contours, cfg.maxVertsPerPoly, *poly),
              "polygon mesh");
    if (!poly->npolys) {
      ++result.empty_tiles;
      if (progress)
        progress(tile + 1, count);
      continue;
    }
    require(poly->nverts < 65535 && poly->npolys < 32768,
            "Navmesh tile exceeds polygon index limits");
    // Normalize the unpadded grid origin before detail sampling. Recast's
    // float subtraction/addition of padding otherwise drifts at large world
    // coordinates with fractional voxel sizes.
    poly->bmin[0] = static_cast<float>(tx * tile_width);
    poly->bmin[2] = static_cast<float>(ty * tile_width);
    poly->bmax[0] = static_cast<float>((tx + 1) * tile_width);
    poly->bmax[2] = static_cast<float>((ty + 1) * tile_width);
    ctx.check(rcBuildPolyMeshDetail(&ctx, *poly, *chf, cfg.detailSampleDist,
                                    cfg.detailSampleMaxError, *detail),
              "detail mesh");
    detail::validate_detail_limits(
        {poly->polys, std::size_t(poly->npolys) * poly->nvp * 2}, poly->nvp,
        {detail->meshes, std::size_t(poly->npolys) * 4});
    for (int i = 0; i < poly->npolys; ++i) {
      poly->areas[i] = 0;
      poly->flags[i] = 1;
    }
    dtNavMeshCreateParams p{};
    p.verts = poly->verts;
    p.vertCount = poly->nverts;
    p.polys = poly->polys;
    p.polyAreas = poly->areas;
    p.polyFlags = poly->flags;
    p.polyCount = poly->npolys;
    p.nvp = 6;
    p.detailMeshes = detail->meshes;
    p.detailVerts = detail->verts;
    p.detailVertsCount = detail->nverts;
    p.detailTris = detail->tris;
    p.detailTriCount = detail->ntris;
    p.walkableHeight = s.actor_height;
    p.walkableRadius = s.actor_radius;
    p.walkableClimb = s.max_climb;
    p.tileX = tx;
    p.tileY = ty;
    std::copy(poly->bmin, poly->bmin + 3, p.bmin);
    std::copy(poly->bmax, poly->bmax + 3, p.bmax);
    p.cs = s.cell_size;
    p.ch = s.cell_height;
    p.buildBvTree = true;
    unsigned char *data = nullptr;
    int size = 0;
    require(dtCreateNavMeshData(&p, &data, &size), "Cannot create Detour tile");
    // Detour converts each local ushort coordinate using two float operations.
    // Use one global-grid rounding instead, so adjacent tile portal vertices
    // are bit-identical (Detour requires their planes within 0.01 units).
    auto *world_vertices =
        reinterpret_cast<float *>(data + sizeof(dtMeshHeader));
    for (int v = 0; v < poly->nverts; ++v) {
      world_vertices[v * 3] = static_cast<float>(
          (double(tx) * s.tile_cells + poly->verts[v * 3]) * s.cell_size);
      world_vertices[v * 3 + 2] = static_cast<float>(
          (double(ty) * s.tile_cells + poly->verts[v * 3 + 2]) * s.cell_size);
    }
    const auto status =
        result.mesh->addTile(data, size, DT_TILE_FREE_DATA, 0, nullptr);
    if (dtStatusFailed(status)) {
      dtFree(data);
      throw std::runtime_error("Cannot add Detour tile");
    }
    ++result.tiles;
    result.polygons += poly->npolys;
    if (progress)
      progress(tile + 1, count);
  }
  check_cancel(cancel);
  require(result.polygons > 0, "Navmesh contains no walkable polygons");
  return result;
}
} // namespace navmesh

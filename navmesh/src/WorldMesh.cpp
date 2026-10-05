#include <DetourAlloc.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <navmesh/WorldMesh.h>
#include <set>
#include <tuple>
namespace navmesh {
namespace {
void need(bool ok, const char *text) {
  if (!ok)
    throw std::runtime_error(text);
}
void check(const Cancel &cancel) {
  if (cancel && cancel())
    throw Cancelled{};
}
} // namespace
WorldMesh load_world(std::span<const RegionFile> files, const Cancel &cancel) {
  need(!files.empty() && files.size() <= 1024,
       "Navmesh world requires 1..1024 regions");
  check(cancel);
  std::vector<RegionFile> ordered(files.begin(), files.end());
  std::sort(ordered.begin(), ordered.end(),
            [](const auto &a, const auto &b) { return a.map < b.map; });
  std::uintmax_t bytes = 0, metadata_bytes = 0;
  std::vector<std::uintmax_t> marker_sizes, mesh_sizes;
  for (std::size_t i = 0; i < ordered.size(); ++i) {
    region_bounds(ordered[i].map);
    need(i == 0 || ordered[i - 1].map != ordered[i].map,
         "Duplicate navmesh region");
    const auto size = std::filesystem::file_size(ordered[i].mesh_path);
    need(size <= 512ULL * 1024 * 1024 && bytes <= 512ULL * 1024 * 1024 - size,
         "Navmesh world exceeds 512 MiB input limit");
    bytes += size;
    mesh_sizes.push_back(size);
    const auto marker_size =
        std::filesystem::file_size(metadata_path(ordered[i].mesh_path));
    need(marker_size <= 16ULL * 1024 * 1024 &&
             metadata_bytes <= 64ULL * 1024 * 1024 - marker_size,
         "Navmesh world exceeds metadata input limit");
    metadata_bytes += marker_size;
    marker_sizes.push_back(marker_size);
  }
  WorldMesh out;
  dtNavMeshParams params{};
  std::map<std::string, SourcePackage> sources;
  std::map<std::tuple<int, int, int>, std::vector<unsigned char>> tiles;
  std::size_t input_index = 0;
  for (const auto &file : ordered) {
    check(cancel);
    const auto marker_before =
        territory::file_identity(metadata_path(file.mesh_path));
    need(marker_before.bytes == marker_sizes[input_index],
         "Navmesh metadata changed after preflight");
    auto m = read_region_metadata(file.mesh_path);
    need(m.mesh.bytes == mesh_sizes[input_index++],
         "Navmesh size changed after preflight");
    need(m.map == file.map, "Navmesh metadata belongs to another region");
    if (!out.regions.empty())
      need(region_profile(m.settings) ==
               region_profile(out.regions.front().settings),
           "Incompatible navmesh generation profiles");
    for (const auto &source : m.sources) {
      const auto [it, inserted] = sources.emplace(source.relative_path, source);
      need(inserted || (it->second.size == source.size &&
                        it->second.sha256 == source.sha256),
           "Incompatible navmesh source packages");
    }
    auto mesh = load(file.mesh_path);
    const auto after = territory::file_identity(file.mesh_path),
               marker_after =
                   territory::file_identity(metadata_path(file.mesh_path));
    need(after.bytes == m.mesh.bytes && after.sha256 == m.mesh.sha256 &&
             marker_before.bytes == marker_after.bytes &&
             marker_before.sha256 == marker_after.sha256,
         "Navmesh input changed while loading world");
    const double width = double(m.settings.cell_size) * m.settings.tile_cells;
    const auto *p = mesh->getParams();
    params = *p;
    need(p->orig[0] == 0 && p->orig[1] == 0 && p->orig[2] == 0 &&
             p->tileWidth == width && p->tileHeight == width,
         "Navmesh tile grid differs from metadata");
    const auto bounds = region_bounds(m.map);
    const auto &const_mesh = static_cast<const dtNavMesh &>(*mesh);
    for (int i = 0; i < mesh->getMaxTiles(); ++i) {
      check(cancel);
      const auto *tile = const_mesh.getTile(i);
      if (!tile->header)
        continue;
      const auto &h = *tile->header;
      need(h.x * width >= bounds.x && (h.x + 1) * width <= bounds.z &&
               h.y * width >= bounds.y && (h.y + 1) * width <= bounds.w,
           "Navmesh tile lies outside claimed region");
      need(h.walkableHeight == m.settings.actor_height &&
               h.walkableRadius == m.settings.actor_radius &&
               h.walkableClimb == m.settings.max_climb,
           "Navmesh tile actor profile differs from metadata");
      for (int v = 0; v < h.vertCount; ++v)
        need(tile->verts[v * 3] >= bounds.x && tile->verts[v * 3] <= bounds.z &&
                 tile->verts[v * 3 + 2] >= bounds.y &&
                 tile->verts[v * 3 + 2] <= bounds.w,
             "Navmesh polygon lies outside claimed region");
      const auto [it, inserted] =
          tiles.try_emplace(std::tuple{h.x, h.y, h.layer});
      need(inserted, "Duplicate navmesh tile ownership");
      need(tiles.size() <= 65536, "Navmesh world exceeds 65536 tiles");
      it->second.assign(tile->data, tile->data + tile->dataSize);
    }
    out.regions.push_back(std::move(m));
    // Keep validated occupied blobs only. Release this input's potentially
    // sparse maxTiles table before loading the next region.
  }
  // Every source map in the intersection of two adjacent 3x3 neighborhoods
  // can affect their common side (including its two corners).
  for (std::size_t i = 0; i < out.regions.size(); ++i)
    for (std::size_t j = i + 1; j < out.regions.size(); ++j) {
      const auto &a = out.regions[i];
      const auto &b = out.regions[j];
      const auto ba = region_bounds(a.map), bb = region_bounds(b.map);
      if (std::abs(ba.x - bb.x) + std::abs(ba.y - bb.y) != 32768)
        continue;
      auto na = region_neighbors(a.map), nb = region_neighbors(b.map);
      na.push_back(a.map);
      nb.push_back(b.map);
      for (const auto &name : na)
        if (std::find(nb.begin(), nb.end(), name) != nb.end()) {
          auto loaded = [&](const auto &r) {
            return name == r.map || std::find(r.loaded_neighbors.begin(),
                                              r.loaded_neighbors.end(),
                                              name) != r.loaded_neighbors.end();
          };
          need(loaded(a) && loaded(b),
               "Incomplete shared boundary context; regenerate both regions "
               "with available neighbors");
        }
    }
  need(!tiles.empty(), "Navmesh world contains no tiles");
  out.mesh.reset(dtAllocNavMesh());
  need(bool(out.mesh), "Cannot allocate world navmesh");
  params.maxTiles = static_cast<int>(tiles.size());
  params.maxPolys = 1 << 20;
  need(dtStatusSucceed(out.mesh->init(&params)),
       "Cannot initialize world navmesh");
  for (const auto &[key, blob] : tiles) {
    (void)key;
    check(cancel);
    auto *data =
        static_cast<unsigned char *>(dtAlloc(blob.size(), DT_ALLOC_PERM));
    need(data != nullptr, "Cannot allocate world navmesh tile");
    std::memcpy(data, blob.data(), blob.size());
    // Zero lastRef requests fresh runtime refs. Detour reconnects portals.
    if (dtStatusFailed(out.mesh->addTile(data, static_cast<int>(blob.size()),
                                         DT_TILE_FREE_DATA, 0, nullptr))) {
      dtFree(data);
      throw std::runtime_error("Cannot add world navmesh tile");
    }
  }
  return out;
}
} // namespace navmesh

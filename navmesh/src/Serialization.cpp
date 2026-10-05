#include <DetourAlloc.h>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <navmesh/Navmesh.h>
#include <random>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace navmesh {
namespace {
static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(dtPolyRef) == 8 && sizeof(dtMeshHeader) == 100 &&
              sizeof(dtPoly) == 32 && sizeof(dtPolyDetail) == 12 &&
              sizeof(dtLink) == 16 && sizeof(dtBVNode) == 16);
constexpr std::uint32_t magic = 0x4d534554, version = 1;
constexpr std::uintmax_t max_file = 512ULL * 1024 * 1024,
                         max_tile = 32ULL * 1024 * 1024;
void need(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(std::string("Invalid navmesh: ") + message);
}
bool grid_coordinate_matches(float actual, double expected) {
  // Padded Recast bounds are rounded to float, then padding is removed in
  // float once more. Accept that bounded precision loss, not a fixed world
  // epsilon which rejects fractional cells far from origin.
  const double tolerance = 4 * std::numeric_limits<float>::epsilon() *
                           std::max(1.0, std::abs(expected));
  return std::abs(double(actual) - expected) <= tolerance;
}
template <class T> T read(std::istream &in) {
  T v{};
  in.read(reinterpret_cast<char *>(&v), sizeof(v));
  need(bool(in), "truncated file");
  return v;
}
template <class T> void write(std::ostream &out, T v) {
  out.write(reinterpret_cast<const char *>(&v), sizeof(v));
}
// Validate our ground-polygon profile before Detour interprets its raw arrays.
void validate_blob(const unsigned char *data, std::size_t size) {
  need(size >= sizeof(dtMeshHeader), "short tile header");
  dtMeshHeader h{};
  std::memcpy(&h, data, sizeof(h));
  need(h.magic == DT_NAVMESH_MAGIC && h.version == 7, "tile format version");
  need(h.polyCount > 0 && h.polyCount < 32768 && h.vertCount >= 3 &&
           h.vertCount < 65535 && h.maxLinkCount > 0 &&
           h.maxLinkCount <= h.polyCount * 24 &&
           h.detailMeshCount == h.polyCount && h.detailVertCount >= 0 &&
           h.detailVertCount <= h.polyCount * 255 &&
           h.detailVertCount <= 65535 && h.detailTriCount >= h.polyCount &&
           h.detailTriCount <= h.polyCount * 255 && h.bvNodeCount >= 0 &&
           h.bvNodeCount <= h.polyCount * 2 && h.offMeshConCount == 0 &&
           h.offMeshBase == h.polyCount && h.layer == 0,
       "tile array counts or unsupported off-mesh profile");
  need(std::abs(double(h.x)) < 10000000 && std::abs(double(h.y)) < 10000000,
       "tile coordinates");
  for (float v :
       {h.walkableHeight, h.walkableRadius, h.walkableClimb, h.bvQuantFactor})
    need(std::isfinite(v) && v >= 0, "nonfinite tile settings");
  need(h.walkableHeight > 0 && h.bvQuantFactor > 0, "zero tile settings");
  for (int i = 0; i < 3; ++i)
    need(std::isfinite(h.bmin[i]) && std::isfinite(h.bmax[i]) &&
             h.bmax[i] >= h.bmin[i] && std::abs(h.bmin[i]) < 10000000 &&
             std::abs(h.bmax[i]) < 10000000,
         "tile bounds");
  std::size_t offset = sizeof(h);
  auto take = [&](std::size_t n, std::size_t element) {
    const auto length = n * element;
    need(offset <= size && length <= size - offset, "tile arrays exceed blob");
    const auto *p = data + offset;
    offset += length;
    return p;
  };
  auto verts = reinterpret_cast<const float *>(take(h.vertCount, 12));
  auto polys =
      reinterpret_cast<const dtPoly *>(take(h.polyCount, sizeof(dtPoly)));
  (void)take(h.maxLinkCount, sizeof(dtLink));
  auto details = reinterpret_cast<const dtPolyDetail *>(
      take(h.detailMeshCount, sizeof(dtPolyDetail)));
  auto detail_verts =
      reinterpret_cast<const float *>(take(h.detailVertCount, 12));
  auto tris = take(h.detailTriCount, 4);
  auto nodes =
      reinterpret_cast<const dtBVNode *>(take(h.bvNodeCount, sizeof(dtBVNode)));
  need(offset == size, "tile trailing bytes");
  for (int i = 0; i < h.vertCount * 3; ++i)
    need(std::isfinite(verts[i]) && std::abs(verts[i]) < 10000000,
         "invalid vertex");
  for (int i = 0; i < h.detailVertCount * 3; ++i)
    need(std::isfinite(detail_verts[i]) && std::abs(detail_verts[i]) < 10000000,
         "invalid detail vertex");
  for (int i = 0; i < h.polyCount; ++i) {
    const auto &p = polys[i];
    const auto &d = details[i];
    need(p.vertCount >= 3 && p.vertCount <= 6 &&
             p.getType() == DT_POLYTYPE_GROUND,
         "polygon type or vertex count");
    for (int j = 0; j < p.vertCount; ++j) {
      need(p.verts[j] < h.vertCount, "polygon vertex out of bounds");
      auto n = p.neis[j];
      need((n & DT_EXT_LINK)
               ? (n == 0x8000 || n == 0x8002 || n == 0x8004 || n == 0x8006)
               : n <= h.polyCount,
           "polygon neighbor out of bounds");
    }
    need(std::uint64_t(d.vertBase) + d.vertCount <=
                 std::uint64_t(h.detailVertCount) &&
             std::uint64_t(d.triBase) + d.triCount <=
                 std::uint64_t(h.detailTriCount) &&
             d.triCount > 0,
         "detail range out of bounds");
    for (unsigned t = d.triBase; t < d.triBase + d.triCount; ++t)
      for (int j = 0; j < 3; ++j)
        need(tris[t * 4 + j] < p.vertCount + d.vertCount,
             "detail triangle index out of bounds");
  }
  for (int i = 0; i < h.bvNodeCount; ++i) {
    const auto &n = nodes[i];
    need(n.i >= 0 ? n.i < h.polyCount
                  : (-std::int64_t(n.i) > 1 &&
                     -std::int64_t(n.i) <= h.bvNodeCount - i),
         "bounding tree index out of bounds");
    for (int j = 0; j < 3; ++j)
      need(n.bmin[j] <= n.bmax[j], "bounding tree bounds");
  }
}
} // namespace

Mesh load(const std::filesystem::path &path) {
  const auto file_size = std::filesystem::file_size(path);
  need(file_size >= 40 && file_size <= max_file, "file size limit");
  std::ifstream in(path, std::ios::binary);
  need(read<std::uint32_t>(in) == magic && read<std::uint32_t>(in) == version,
       "MSET magic/version");
  const auto tiles = read<std::uint32_t>(in);
  dtNavMeshParams params{};
  for (float &v : params.orig) {
    v = read<float>(in);
    need(std::isfinite(v) && v == 0, "unsupported nonzero origin");
  }
  params.tileWidth = read<float>(in);
  params.tileHeight = read<float>(in);
  params.maxTiles = read<std::int32_t>(in);
  params.maxPolys = read<std::int32_t>(in);
  need(std::isfinite(params.tileWidth) && params.tileWidth >= 8 &&
           params.tileWidth <= 16384 && params.tileHeight == params.tileWidth &&
           params.maxTiles > 0 && params.maxTiles <= 65536 &&
           params.maxPolys > 0 && params.maxPolys <= (1 << 20) && tiles > 0 &&
           tiles <= unsigned(params.maxTiles),
       "mesh parameters or tile count");
  Mesh mesh{dtAllocNavMesh()};
  need(bool(mesh), "mesh allocation");
  need(dtStatusSucceed(mesh->init(&params)), "mesh initialization");
  std::uintmax_t consumed = 40;
  for (unsigned i = 0; i < tiles; ++i) {
    const auto ref = read<std::uint64_t>(in);
    const auto size = read<std::uint32_t>(in);
    need(read<std::uint32_t>(in) == 0, "tile header padding");
    consumed += 16;
    need(ref != 0 && mesh->decodePolyIdPoly(ref) == 0 &&
             mesh->decodePolyIdSalt(ref) != 0 &&
             mesh->decodePolyIdTile(ref) < unsigned(params.maxTiles),
         "tile reference");
    need(size >= 100 && size <= max_tile && consumed <= file_size &&
             size <= file_size - consumed,
         "tile length");
    std::unique_ptr<unsigned char, decltype(&dtFree)> data(
        static_cast<unsigned char *>(dtAlloc(size, DT_ALLOC_PERM)), dtFree);
    need(bool(data), "tile allocation");
    in.read(reinterpret_cast<char *>(data.get()), size);
    need(bool(in), "truncated tile");
    validate_blob(data.get(), size);
    const auto *h = reinterpret_cast<const dtMeshHeader *>(data.get());
    need(grid_coordinate_matches(h->bmin[0], double(h->x) * params.tileWidth) &&
             grid_coordinate_matches(h->bmin[2],
                                     double(h->y) * params.tileHeight),
         "tile grid does not match bounds");
    need(dtStatusSucceed(mesh->addTile(data.get(), int(size), DT_TILE_FREE_DATA,
                                       ref, nullptr)),
         "duplicate or invalid tile");
    data.release();
    consumed += size;
  }
  need(consumed == file_size && in.peek() == std::char_traits<char>::eof(),
       "file trailing bytes");
  return mesh;
}

void save(const dtNavMesh &mesh, const std::filesystem::path &path,
          const Cancel &cancel) {
  const auto check_cancel = [&] {
    if (cancel && cancel())
      throw Cancelled{};
  };
  check_cancel();
  if (std::filesystem::exists(path))
    throw std::runtime_error("Navmesh output already exists");
  std::random_device random;
  const auto scratch =
      path.parent_path() /
      (".navmesh-" + std::to_string(random()) + "-" + std::to_string(random()));
  if (!std::filesystem::create_directory(scratch))
    throw std::runtime_error("Cannot reserve navmesh temporary directory");
  const auto temporary = scratch / "mesh.part";
  auto cleanup = [&] {
    std::error_code ec;
    std::filesystem::remove(temporary, ec);
    std::filesystem::remove(scratch, ec);
  };
  try {
    std::uint32_t count = 0;
    for (int i = 0; i < mesh.getMaxTiles(); ++i)
      if (mesh.getTile(i)->header)
        ++count;
    const auto &params = *mesh.getParams();
    std::ofstream out(temporary, std::ios::binary);
    write(out, magic);
    write(out, version);
    write(out, count);
    for (float v : params.orig)
      write(out, v);
    write(out, params.tileWidth);
    write(out, params.tileHeight);
    write(out, std::int32_t(params.maxTiles));
    write(out, std::int32_t(params.maxPolys));
    for (int i = 0; i < mesh.getMaxTiles(); ++i) {
      check_cancel();
      const auto *tile = mesh.getTile(i);
      if (!tile->header)
        continue;
      write(out, std::uint64_t(mesh.getTileRef(tile)));
      write(out, std::uint32_t(tile->dataSize));
      write(out, std::uint32_t{0});
      out.write(reinterpret_cast<const char *>(tile->data), tile->dataSize);
    }
    out.close();
    if (!out)
      throw std::runtime_error("Failed to write navmesh file");
    check_cancel();
    (void)load(temporary);
    check_cancel();
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
      throw std::system_error(int(GetLastError()), std::system_category(),
                              "publish navmesh");
#else
    // Exclusive publication: unlike POSIX rename, link cannot replace a
    // destination.
    std::filesystem::create_hard_link(temporary, path);
#endif
    cleanup();
  } catch (...) {
    cleanup();
    throw;
  }
}
} // namespace navmesh

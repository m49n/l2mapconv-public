#include "L2JSerializer.h"
#include <DetourNavMeshQuery.h>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <pathfinding/Dataset.h>
#include <sstream>
#include <stdexcept>
#include <territory/PathIO.h>
#include <navmesh/WorldMesh.h>
#include <set>
namespace pathfinding {
namespace {
// The legacy decoder assumes a complete valid stream. Validate an immutable,
// bounded byte snapshot first, then reuse that decoder on the SAME bytes.
std::string l2j_snapshot(const FileIdentity &input) {
  constexpr std::size_t max_bytes = 128 * 1024 * 1024,
                        max_cells = 16 * 1024 * 1024;
  if (input.size > max_bytes)
    throw std::invalid_argument("L2J exceeds 128 MiB limit");
  std::ifstream file(input.path, std::ios::binary);
  if (!file)
    throw std::runtime_error("Cannot open L2J dataset");
  std::string bytes(static_cast<std::size_t>(input.size), '\0');
  file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  if (!file || file.peek() != std::char_traits<char>::eof())
    throw std::runtime_error("L2J size changed");
  std::size_t offset = 0, cells = 0;
  const auto take = [&](std::size_t n) {
    if (n > bytes.size() - offset)
      throw std::invalid_argument("Truncated L2J block");
    const auto start = offset;
    offset += n;
    return start;
  };
  for (int block = 0; block < 256 * 256; ++block) {
    const auto type = static_cast<unsigned char>(bytes[take(1)]);
    if (type == 0) {
      take(2);
      ++cells;
    } else if (type == 1) {
      take(128);
      cells += 64;
    } else if (type == 2) {
      for (int column = 0; column < 64; ++column) {
        const auto layers = static_cast<unsigned char>(bytes[take(1)]);
        take(layers * 2);
        cells += layers;
      }
    } else
      throw std::invalid_argument("Unknown L2J block type");
    if (cells > max_cells)
      throw std::invalid_argument("L2J exceeds cell limit");
  }
  if (offset != bytes.size())
    throw std::invalid_argument("Trailing data after L2J region");
  return bytes;
}
unsigned nswe(const geodata::Cell &c) {
  return (c.north ? 8u : 0u) | (c.south ? 4u : 0u) | (c.west ? 2u : 0u) |
         (c.east ? 1u : 0u);
}
void validate_slice(double low, double high) {
  if (!std::isfinite(low) || !std::isfinite(high) || low > high)
    throw std::invalid_argument("Invalid height slice");
}
struct PolygonCollector : dtPolyQuery {
  std::vector<dtPolyRef> refs;
  bool overflow{};
  void process(const dtMeshTile *, dtPoly **, dtPolyRef *polys,
               int count) override {
    if (refs.size() + count > 4096) {
      overflow = true;
      return;
    }
    refs.insert(refs.end(), polys, polys + count);
  }
};
} // namespace
Dataset Dataset::load(std::string map, const FileIdentity &l2j,
                      const FileIdentity &nav) {
  const std::vector<NavRegionInput> regions=has_input(nav)?std::vector<NavRegionInput>{{map,nav,{}}}:std::vector<NavRegionInput>{};
  return load(std::move(map),l2j,regions);
}
Dataset Dataset::load(std::string map, const FileIdentity &l2j,
                      const std::vector<NavRegionInput>& regions) {
  const auto start = std::chrono::steady_clock::now();
  if (!has_input(l2j) && regions.empty())
    throw std::invalid_argument("Select at least one navigation format");
  if (has_input(l2j))
    verify_identity(l2j);
  if(regions.size()>1024)throw std::invalid_argument("Too many navigation regions");
  const bool legacy=regions.size()==1 && !has_input(regions.front().metadata);
  std::set<std::string> names;
  for(const auto& r:regions) {
    region_origin(r.map);
    if(!names.insert(r.map).second)throw std::invalid_argument("Duplicate navigation region");
    verify_identity(r.mesh);
    if(!legacy) {
      verify_identity(r.metadata);
      if(r.metadata.path!=navmesh::metadata_path(r.mesh.path))throw std::invalid_argument("Metadata must accompany mesh");
    }
  }
  Dataset d;
  if(!has_input(l2j) && !regions.empty()) map=regions.front().map;
  d.m_map = std::move(map);
  d.m_origin = region_origin(d.m_map);
  d.m_l2j = l2j;
  if(legacy)d.m_nav=regions.front().mesh;
  else d.m_nav_regions=regions;
  if(has_input(l2j)) names.insert(d.m_map);
  d.m_regions.assign(names.begin(),names.end());
  if (has_input(l2j)) {
    std::istringstream bytes(l2j_snapshot(l2j), std::ios::binary);
    d.m_geo = geodata::L2JSerializer{}.deserialize(bytes);
    // The Lab's offline L2J engine masks flat-block heights to 16 units
    // (GeoEngine.NgetHeight). Packed complex/multilayer heights use 8 units.
    // Match the backend's surface for picking; never rewrite the input file.
    for (auto &cell : d.m_geo.cells)
      if (cell.type == geodata::BLOCK_SIMPLE)
        cell.z = static_cast<std::int16_t>(static_cast<std::uint16_t>(cell.z) &
                                           0xfff0u);
  }
  d.m_blocks.resize(256 * 256);
  std::size_t index = 0;
  for (int bx = 0; bx < 256; ++bx)
    for (int by = 0; by < 256; ++by) {
      const auto start_index = index;
      while (index < d.m_geo.cells.size() && d.m_geo.cells[index].x / 8 == bx &&
             d.m_geo.cells[index].y / 8 == by)
        ++index;
      d.m_blocks[bx * 256 + by] = {start_index, index};
    }
  if (!regions.empty()) {
    if(legacy)d.m_mesh=navmesh::load(regions.front().mesh.path);
    else {
      std::vector<navmesh::RegionFile> files;
      for(const auto& r:regions)files.push_back({r.map,r.mesh.path});
      d.m_mesh=navmesh::load_world(files).mesh;
    }
    const auto &mesh = *static_cast<const dtNavMesh *>(d.m_mesh.get());
    for (int ti = 0; ti < mesh.getMaxTiles(); ++ti) {
      const auto *tile = mesh.getTile(ti);
      if (!tile->header)
        continue;
      for (int pi = 0; pi < tile->header->polyCount; ++pi) {
        const auto &poly = tile->polys[pi];
        if (poly.getType() != DT_POLYTYPE_GROUND)
          continue;
        std::vector<WorldPoint> points;
        for (int vi = 0; vi < poly.vertCount; ++vi) {
          const auto *v = &tile->verts[poly.verts[vi] * 3];
          WorldPoint p{v[0], v[2], v[1]};
          validate_point(p);
          const auto owner=region_origin(regions.front().map);
          if (legacy && (p.x < owner.x - 0.01 || p.y < owner.y - 0.01 ||
              p.x > owner.x + 32768.01 || p.y > owner.y + 32768.01))
            throw std::invalid_argument(
                "Navmesh polygons do not belong to selected region");
          points.push_back(p);
        }
        d.m_polygons.push_back(std::move(points));
      }
    }
  }
  if (has_input(l2j))
    verify_identity(l2j);
  for(const auto& r:regions){verify_identity(r.mesh);if(!legacy)verify_identity(r.metadata);}
  d.m_load_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
  return d;
}
bool Dataset::contains(double x, double y) const {
  return region_at(x,y).has_value();
}
std::optional<std::string> Dataset::region_at(double x,double y) const {
  if(!std::isfinite(x)||!std::isfinite(y))return {};
  for(const auto& name:m_regions){const auto o=region_origin(name);
    if(x>=o.x && y>=o.y && x<o.x+32768 && y<o.y+32768)return name;}
  return {};
}
glm::dvec4 Dataset::game_bounds() const {
  glm::dvec4 bounds{1e7,1e7,-1e7,-1e7};
  for(const auto& name:m_regions){const auto o=region_origin(name);
    bounds.x=std::min(bounds.x,o.x);bounds.y=std::min(bounds.y,o.y);
    bounds.z=std::max(bounds.z,o.x+32768);bounds.w=std::max(bounds.w,o.y+32768);}
  return bounds;
}
auto Dataset::l2j_overview(glm::dvec4 xy, double low, double high,
                           unsigned step) const -> std::vector<OverviewCell> {
  validate_slice(low, high);
  if (step < 16 || step > 32768 || !std::has_single_bit(step))
    throw std::invalid_argument(
        "Overlay cell width must be a power of two from 16 to 32768");
  for (int i = 0; i < 4; ++i)
    if (!std::isfinite(xy[i]))
      throw std::invalid_argument("Invalid overlay bounds");
  if (xy.x > xy.z || xy.y > xy.w)
    throw std::invalid_argument("Inverted overlay bounds");
  const double x0 = std::max(0., xy.x - m_origin.x),
               y0 = std::max(0., xy.y - m_origin.y),
               x1 = std::min(32768., xy.z - m_origin.x),
               y1 = std::min(32768., xy.w - m_origin.y);
  if (x0 >= x1 || y0 >= y1)
    return {};
  const int gx0 = int(std::floor(x0 / step)), gy0 = int(std::floor(y0 / step)),
            gx1 = int(std::ceil(x1 / step)) - 1,
            gy1 = int(std::ceil(y1 / step)) - 1, width = gx1 - gx0 + 1,
            height = gy1 - gy0 + 1;
  struct Bin {
    unsigned mask{}, count{};
    double z{};
    bool mixed{};
  };
  std::vector<Bin> bins(std::size_t(width) * height);
  // Include the complete bins at viewport edges, so panning does not change
  // a bin's classification merely because another part becomes visible.
  const int bx0 = gx0 * int(step) / 128, by0 = gy0 * int(step) / 128,
            bx1 = ((gx1 + 1) * int(step) - 1) / 128,
            by1 = ((gy1 + 1) * int(step) - 1) / 128;
  for (int bx = bx0; bx <= bx1; ++bx)
    for (int by = by0; by <= by1; ++by) {
      const auto span = m_blocks[bx * 256 + by];
      for (auto i = span.begin; i < span.end; ++i) {
        const auto &cell = m_geo.cells[i];
        if (cell.z < low || cell.z > high)
          continue;
        const int size = cell.type == geodata::BLOCK_SIMPLE ? 128 : 16;
        const int cx0 = std::max(gx0, int(cell.x) * 16 / int(step)),
                  cy0 = std::max(gy0, int(cell.y) * 16 / int(step)),
                  cx1 =
                      std::min(gx1, (int(cell.x) * 16 + size - 1) / int(step)),
                  cy1 =
                      std::min(gy1, (int(cell.y) * 16 + size - 1) / int(step));
        const auto mask = nswe(cell);
        for (int y = cy0; y <= cy1; ++y)
          for (int x = cx0; x <= cx1; ++x) {
            auto &bin = bins[std::size_t(y - gy0) * width + x - gx0];
            if (bin.count)
              bin.mixed |= bin.mask != mask;
            else {
              bin.mask = mask;
              bin.z = cell.z;
            }
            ++bin.count;
          }
      }
    }
  std::vector<OverviewCell> out;
  out.reserve(bins.size());
  for (int y = gy0; y <= gy1; ++y)
    for (int x = gx0; x <= gx1; ++x) {
      const auto &bin = bins[std::size_t(y - gy0) * width + x - gx0];
      if (bin.count)
        out.push_back({{m_origin.x + x * step, m_origin.y + y * step, bin.z},
                       double(step),
                       bin.mask,
                       bin.mixed,
                       step == 16 && bin.count == 1});
    }
  return out;
}
auto Dataset::candidates(double x, double y) const
    -> std::vector<ResolvedEndpoint> {
  std::vector<ResolvedEndpoint> out;
  if (!contains(x, y))
    return out;
  if(has_input(m_l2j) && x>=m_origin.x && y>=m_origin.y && x<m_origin.x+32768 && y<m_origin.y+32768) {
  const int cx = static_cast<int>((x - m_origin.x) / 16),
            cy = static_cast<int>((y - m_origin.y) / 16);
  const auto span = m_blocks[(cx / 8) * 256 + cy / 8];
  for (auto i = span.begin; i < span.end; ++i) {
    const auto &c = m_geo.cells[i];
    if (c.type != geodata::BLOCK_SIMPLE && (c.x != cx || c.y != cy))
      continue;
    WorldPoint p{x, y, static_cast<double>(c.z)};
    out.push_back({p, p, nswe(c) != 0,
                   "l2j:" + std::to_string(cx) + ":" + std::to_string(cy) +
                       ":" + std::to_string(i) +
                       ":nswe=" + std::to_string(nswe(c))});
  }
  }
  if (m_mesh) {
    dtNavMeshQuery query;
    if (dtStatusFailed(query.init(m_mesh.get(), 1)))
      throw std::runtime_error("Cannot initialize surface query");
    const float pos[]{static_cast<float>(x), 0, static_cast<float>(y)},
        extents[]{0.01f, 10000000, 0.01f};
    dtQueryFilter filter;
    PolygonCollector found;
    if (dtStatusFailed(query.queryPolygons(pos, extents, &filter, &found)) ||
        found.overflow)
      throw std::runtime_error("Too many navigation surfaces in one column");
    for (auto ref : found.refs) {
      bool over = false;
      float closest[3]{}, height{};
      if (dtStatusFailed(query.closestPointOnPoly(ref, pos, closest, &over)) ||
          !over || dtStatusFailed(query.getPolyHeight(ref, closest, &height)))
        continue;
      WorldPoint p{x, y, height};
      out.push_back({p, p, true, "nav:" + std::to_string(ref)});
    }
  }
  std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) {
    if (a.resolved.z != b.resolved.z)
      return a.resolved.z < b.resolved.z;
    return a.surface_id < b.surface_id;
  });
  return out;
}
auto Dataset::nav_polygons(double low, double high) const
    -> std::vector<std::vector<WorldPoint>> {
  validate_slice(low, high);
  std::vector<std::vector<WorldPoint>> out;
  for (const auto &p : m_polygons) {
    double min = 10000000, max = -10000000;
    for (auto v : p) {
      min = std::min(min, v.z);
      max = std::max(max, v.z);
    }
    if (min <= high && max >= low)
      out.push_back(p);
  }
  return out;
}
auto Dataset::l2j_cells(glm::dvec4 xy, double low, double high,
                        std::size_t limit) const -> std::vector<VisibleCell> {
  validate_slice(low, high);
  for (int i = 0; i < 4; ++i)
    if (!std::isfinite(xy[i]))
      throw std::invalid_argument("Invalid overlay bounds");
  if (xy.x > xy.z || xy.y > xy.w)
    throw std::invalid_argument("Inverted overlay bounds");
  limit = std::min<std::size_t>(limit, 65536);
  std::vector<VisibleCell> out;
  if (xy.z < m_origin.x || xy.w < m_origin.y || xy.x >= m_origin.x + 32768 ||
      xy.y >= m_origin.y + 32768)
    return out;
  const auto block = [](double p, double origin) {
    return static_cast<int>(
        std::clamp(std::floor((p - origin) / 128), 0.0, 255.0));
  };
  for (int bx = block(xy.x, m_origin.x); bx <= block(xy.z, m_origin.x); ++bx)
    for (int by = block(xy.y, m_origin.y); by <= block(xy.w, m_origin.y);
         ++by) {
      const auto span = m_blocks[bx * 256 + by];
      for (auto i = span.begin; i < span.end; ++i) {
        if (out.size() == limit)
          return out;
        const auto &c = m_geo.cells[i];
        if (c.z < low || c.z > high)
          continue;
        const WorldPoint minimum{m_origin.x + c.x * 16, m_origin.y + c.y * 16,
                                 static_cast<double>(c.z)};
        const double width = c.type == geodata::BLOCK_SIMPLE ? 128 : 16;
        if (minimum.x > xy.z || minimum.y > xy.w || minimum.x + width <= xy.x ||
            minimum.y + width <= xy.y)
          continue;
        out.push_back({minimum, width, nswe(c)});
      }
    }
  return out;
}
} // namespace pathfinding

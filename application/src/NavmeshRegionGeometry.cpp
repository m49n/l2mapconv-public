#include "NavmeshRegionGeometry.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <territory/PathIO.h>
namespace {
void check(const navmesh::Cancel &cancel) {
  if (cancel && cancel())
    throw navmesh::Cancelled{};
}
} // namespace
void verify_navmesh_sources(const std::filesystem::path &client,
                            const navmesh::RegionMetadata &metadata,
                            const navmesh::Cancel &cancel) {
  for (const auto &source : metadata.sources) {
    check(cancel);
    const auto actual = territory::file_identity(
        client / territory::path_from_utf8(source.relative_path));
    if (actual.bytes != source.size || actual.sha256 != source.sha256)
      throw std::runtime_error("Navmesh source package changed: " +
                               source.relative_path);
  }
}
NavmeshRegionGeometry load_navmesh_region_geometry(
    const std::filesystem::path &client, const std::string &name,
    const navmesh::Settings &settings, const navmesh::Cancel &cancel,
    const CollisionMapLoad &load) {
  navmesh::validate_region_grid(settings);
  check(cancel);
  const auto bounds = navmesh::region_bounds(name);
  NavmeshRegionGeometry result{
      navmesh::InputGeometry{geometry::Box{{bounds.x, bounds.y, -32768},
                                           {bounds.z, bounds.w, 32768}}},
      {}};
  auto &metadata = result.metadata;
  metadata.map = name;
  metadata.settings = settings;
  metadata.neighbor_context = true;
  metadata.padding = navmesh::context_padding(settings);
  std::map<std::string, territory::FileIdentity> sources, pending;
  auto observe = [&](const std::filesystem::path &path,
                     unreal::ArchiveReadPhase phase) {
    check(cancel);
    if (!territory::is_within(path, client))
      throw std::runtime_error("Navmesh package lies outside client");
    auto key = territory::path_utf8(std::filesystem::relative(path, client));
    std::replace(key.begin(), key.end(), '\\', '/');
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    const auto identity = territory::file_identity(path);
    auto equal = [&](const auto &old) {
      return old.bytes == identity.bytes && old.sha256 == identity.sha256;
    };
    if (auto old = sources.find(key);
        old != sources.end() && !equal(old->second))
      throw std::runtime_error(
          "Navmesh source package changed between loads: " + key);
    if (phase == unreal::ArchiveReadPhase::Before)
      pending[key] = identity;
    else {
      const auto before = pending.find(key);
      if (before == pending.end() || !equal(before->second))
        throw std::runtime_error(
            "Navmesh source package changed while reading: " + key);
      sources[key] = identity;
      pending.erase(before);
    }
  };
  auto names = navmesh::region_neighbors(name);
  names.insert(names.begin(), name);
  for (const auto &current : names) {
    check(cancel);
    const auto file = client / "Maps" / (current + ".unr");
    if (!std::filesystem::exists(file) && current != name) {
      metadata.missing_neighbors.push_back(current);
      continue;
    }
    if (!std::filesystem::is_regular_file(file))
      throw std::runtime_error("Navmesh map input is not readable: " + current);
    // A fresh loader per map releases package and mesh caches before the next.
    auto map = load(current, observe);
    if (map.vertices().empty() || map.indices().empty())
      throw std::runtime_error("Empty navmesh collision map: " + current);
    if (current == name) {
      const auto lo = map.bounding_box().min(), hi = map.bounding_box().max();
      if (std::abs(lo.x - bounds.x) > .1 || std::abs(lo.y - bounds.y) > .1 ||
          std::abs(hi.x - bounds.z) > .1 || std::abs(hi.y - bounds.w) > .1)
        throw std::runtime_error(
            "Navmesh geometry bounds do not match requested square");
    }
    navmesh::append_context(result.geometry, map, metadata.padding, cancel);
    if (current != name)
      metadata.loaded_neighbors.push_back(current);
  }
  if (!pending.empty())
    throw std::runtime_error("Incomplete navmesh package read");
  for (const auto &[path, id] : sources)
    metadata.sources.push_back({path, id.sha256, id.bytes});
  verify_navmesh_sources(client, metadata, cancel);
  return result;
}

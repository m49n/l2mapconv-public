#include "MapLoadOptions.h"
#include "SceneStats.h"
#include "TestSupport.h"

#include <memory>

namespace {

auto map_with_surface(std::uint64_t type) -> Map {
  Map map;
  auto mesh = std::make_shared<EntityMesh>();
  mesh->vertices.resize(3);
  mesh->indices.resize(3);
  mesh->surfaces.push_back(
      {.type = type, .index_offset = 0, .index_count = 3, .material = {}});
  map.entities.emplace_back(std::move(mesh));
  return map;
}

} // namespace

auto run_map_load_options_tests() -> int {
  auto failures = 0;

  failures += expect(MapLoadOptions::terrain_only() ==
                         MapLoadOptions{true, false, false, false},
                     "terrain preset excludes every detail class");
  failures += expect(MapLoadOptions::detail_only() ==
                         MapLoadOptions{false, true, true, true},
                     "detail preset excludes terrain");
  failures += expect(MapLoadOptions::blocking_only() ==
                         MapLoadOptions{false, false, false, true},
                     "live detail keeps only legacy blocking-volume overlays");
  failures += expect(MapLoadOptions::full() ==
                         MapLoadOptions{true, true, true, true},
                     "full preset preserves eager build behavior");

  const auto terrain = map_with_surface(SURFACE_TERRAIN | SURFACE_PASSABLE);
  failures += expect(map_contains_only(terrain, SURFACE_TERRAIN),
                     "terrain purity accepts passable modifier bits");

  const auto detail = map_with_surface(SURFACE_STATIC_MESH | SURFACE_PASSABLE);
  failures += expect(map_contains_only(
                         detail, SURFACE_STATIC_MESH | SURFACE_CSG |
                                     SURFACE_BLOCKING_VOLUME),
                     "detail purity accepts every detail base class");

  const auto foreign_bounding_box =
      map_with_surface(SURFACE_STATIC_MESH | SURFACE_BOUNDING_BOX);
  failures += expect(!map_contains_only(foreign_bounding_box, SURFACE_TERRAIN),
                     "purity rejects a bounding box with a foreign base class");

  return failures;
}

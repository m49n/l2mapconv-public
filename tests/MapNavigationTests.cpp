#include "MapNavigation.h"
#include "TestSupport.h"
#include <chrono>
#include <fstream>
#include <limits>

auto run_map_navigation_tests() -> int {
  int failures = 0;
  const MapGridTransform grid{{20, 18}, {0, 0}, {32768, 32768}};
  failures += expect(grid.world_center({13, 24}) == glm::vec2{-212992, 212992},
                     "Go maps negative-world square to its center");
  failures += expect(grid.world_center({25, 19}) == glm::vec2{180224, 49152},
                     "Go uses clicked square center, not seed center");
  const auto root = std::filesystem::temp_directory_path() /
      ("map-go-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root / "Maps");
  std::ofstream{root / "Maps/13_24.unr"};
  std::ofstream{root / "Maps/25_19.unr"};
  MapSelectionContext selection{MapCatalog::discover(root)};
  selection.set_manual({25, 19}, true);
  selection.set_auto_load(false);
  selection.set_include_neighbors(false);
  failures += expect(!map_focus_target(selection, {13,24}, 0), "Go unavailable before grid initialized");
  selection.set_grid(grid);
  const auto manual = selection.manual_selection();
  failures += expect(map_focus_target(selection,{13,24},0) == glm::vec3{-212992,212992,32768},
                     "unchecked unloaded Go target is independent of checked map");
  failures += expect(selection.manual_selection() == manual && !selection.auto_load() &&
                     !selection.include_neighbors(), "Go preserves map selections and automatic flags");
  selection.set_bounds({25,19}, geometry::Box{{0,0,-100}, {1,1,100}});
  failures += expect(map_focus_target(selection,{25,19},0) == glm::vec3{180224,49152,8292},
                     "known bounds place camera above geometry");
  const auto high_target = map_focus_target(selection,{13,24},40000);
  failures += expect(high_target && high_target->z == 40000,
                     "unloaded Go retains higher camera altitude on subsequent click");
  failures += expect(map_focus_position({1,2}, geometry::Box{{0,0,0}, {1,1,std::numeric_limits<float>::infinity()}},0).z == 32768,
                     "invalid bounds cannot create nonfinite camera");
  failures += expect(!map_focus_target(selection,{99,99},0), "Go rejects unavailable catalog square");
  std::filesystem::remove_all(root);
  return failures;
}

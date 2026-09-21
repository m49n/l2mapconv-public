#include "MapGridTransform.h"
#include "MapResidency.h"
#include "TestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

class SparseCatalogFixture {
public:
  SparseCatalogFixture() {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    root = std::filesystem::temp_directory_path() /
           ("l2mapconv-map-residency-" + suffix);
    std::filesystem::create_directories(root / "Maps");
    touch("12_12.unr");
    touch("22_22.unr");
    touch("22_23.unr");
    touch("23_22.unr");
  }

  ~SparseCatalogFixture() { std::filesystem::remove_all(root); }

  void touch(const char *name) const { std::ofstream file{root / "Maps" / name}; }

  std::filesystem::path root;
};

} // namespace

auto run_map_residency_tests() -> int {
  auto failures = 0;

  const auto grid = MapGridTransform{{22, 22}, {65536.0f, 32768.0f},
                                     {32768.0f, 32768.0f}};
  failures += expect(grid.coordinate_at({65536.0f, 32768.0f}) ==
                         MapCoordinate{22, 22},
                     "anchor minimum belongs to anchor map");
  failures += expect(grid.coordinate_at({98303.99f, 65535.99f}) ==
                         MapCoordinate{22, 22},
                     "upper interior remains in anchor map");
  failures += expect(grid.coordinate_at({98304.0f, 65536.0f}) ==
                         MapCoordinate{23, 23},
                     "exact positive boundary enters next map");
  failures += expect(grid.coordinate_at({65535.99f, 32767.99f}) ==
                         MapCoordinate{21, 21},
                     "negative offset uses floor rather than truncation");

  Map map;
  map.bounding_box = geometry::Box{{10.0f, 20.0f, -5.0f},
                                   {110.0f, 220.0f, 15.0f}};
  const auto from_map = MapGridTransform::from_map({7, 8}, map);
  failures += expect(from_map &&
                         from_map->coordinate_at({109.0f, 219.0f}) ==
                             MapCoordinate{7, 8},
                     "map factory uses bounding-box minimum and extent");

  Map empty_map;
  empty_map.bounding_box =
      geometry::Box{{1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}};
  failures += expect(!MapGridTransform::from_map({7, 8}, empty_map),
                     "map factory rejects nonpositive tile extents");

  const SparseCatalogFixture fixture;
  const auto catalog = MapCatalog::discover(fixture.root);
  MapResidencyIntent intent{
      .manual = {{12, 12}},
      .current = MapCoordinate{22, 22},
      .auto_load = true,
      .include_neighbors = false,
  };
  auto desired = desired_map_residency(catalog, intent);
  failures += expect(desired.terrain == Coordinates{{12, 12}, {22, 22}},
                     "manual and automatic terrain are unioned");
  failures += expect(desired.detail == Coordinates{{22, 22}},
                     "current-only detail contains one map");

  intent.include_neighbors = true;
  desired = desired_map_residency(catalog, intent);
  const auto neighbors = catalog.neighbors({22, 22}, 1);
  failures += expect(desired.detail ==
                         Coordinates{neighbors.begin(), neighbors.end()},
                     "plus-one detail skips catalog holes");

  intent.auto_load = false;
  desired = desired_map_residency(catalog, intent);
  failures += expect(desired.terrain == Coordinates{{12, 12}} &&
                         desired.detail.empty(),
                     "disabled automatic loading keeps only manual terrain");

  intent.auto_load = true;
  intent.current = MapCoordinate{99, 99};
  desired = desired_map_residency(catalog, intent);
  failures += expect(desired.terrain == Coordinates{{12, 12}} &&
                         desired.detail.empty(),
                     "missing current map requests no automatic layers");

  return failures;
}

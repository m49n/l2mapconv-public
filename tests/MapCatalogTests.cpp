#include "MapCatalog.h"
#include "TestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path = std::filesystem::temp_directory_path() /
           ("l2mapconv-map-catalog-" + suffix);
    std::filesystem::create_directories(path / "Maps");
  }

  ~TemporaryDirectory() { std::filesystem::remove_all(path); }

  std::filesystem::path path;
};

void touch(const std::filesystem::path &path) {
  std::ofstream file{path};
}

} // namespace

auto run_map_catalog_tests() -> int {
  auto failures = 0;
  const TemporaryDirectory temporary;
  const auto maps = temporary.path / "Maps";

  touch(maps / "22_22.unr");
  touch(maps / "22_23.unr");
  touch(maps / "24_22.unr");
  touch(maps / "Lobby.unr");
  touch(maps / "22_22.txt");

  const auto catalog = MapCatalog::discover(temporary.path);
  failures += expect(catalog.regions().size() == 3,
                     "catalog includes only numeric UNR maps");
  failures += expect(catalog.contains({22, 22}), "catalog finds a region");
  failures += expect(!catalog.contains({23, 22}), "catalog preserves holes");
  failures += expect(catalog.extents() == MapExtents{22, 24, 22, 23},
                     "catalog reports grid extents");
  failures += expect(catalog.neighbors({22, 22}, 1) ==
                         std::vector<MapCoordinate>{{22, 22}, {22, 23}},
                     "neighbors are clipped to available maps");

  return failures;
}

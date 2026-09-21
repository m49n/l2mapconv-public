#include "MapSelectionContext.h"
#include "TestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

class SelectionCatalogFixture {
public:
  SelectionCatalogFixture() {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    root = std::filesystem::temp_directory_path() /
           ("l2mapconv-map-selection-" + suffix);
    std::filesystem::create_directories(root / "Maps");
    touch("22_22.unr");
    touch("22_23.unr");
    touch("24_22.unr");
  }

  ~SelectionCatalogFixture() { std::filesystem::remove_all(root); }

  void touch(const char *name) const {
    std::ofstream file{root / "Maps" / name};
  }

  std::filesystem::path root;
};

} // namespace

auto run_map_selection_tests() -> int {
  auto failures = 0;
  const SelectionCatalogFixture fixture;
  const auto catalog = MapCatalog::discover(fixture.root);
  MapSelectionContext selection{catalog};

  failures += expect(selection.auto_load() && selection.include_neighbors(),
                     "automatic current and plus-one default to enabled");

  selection.select_all_manual();
  failures +=
      expect(selection.manual_selection().size() == catalog.regions().size(),
             "select all pins every available map");
  selection.clear_manual();
  failures += expect(selection.manual_selection().empty(),
                     "clear manual does not synthesize selections");
  failures += expect(selection.auto_load() && selection.include_neighbors(),
                     "clear manual preserves automatic flags");

  selection.set_manual({22, 22}, true);
  selection.set_current({22, 22});
  selection.set_status({22, 22}, MapLayer::Terrain,
                       MapResidencyStatus::Resident);
  failures += expect(selection.summary().terrain_resident == 1 &&
                         selection.current_label() == "22_22",
                     "selection exposes UI summary and current label");

  selection.set_status({22, 22}, MapLayer::Detail, MapResidencyStatus::Failed,
                       "detail failed");
  failures += expect(selection.status({22, 22}, MapLayer::Detail) ==
                             MapResidencyStatus::Failed &&
                         selection.failure({22, 22}, MapLayer::Detail) ==
                             "detail failed",
                     "selection retains per-layer status and failure text");

  selection.set_current({23, 22});
  failures += expect(selection.current_label() == "outside catalog",
                     "missing current coordinate is reported outside catalog");

  return failures;
}

#include "MapSelectionContext.h"
#include "TerritoryRenderWindow.h"
#include <algorithm>
std::vector<std::string>
selected_render_maps(const MapSelectionContext &selection) {
  std::vector<std::string> maps;
  for (const auto coordinate : selection.manual_selection())
    if (const auto *region = selection.catalog().find(coordinate))
      maps.push_back(region->name);
  std::sort(maps.begin(), maps.end());
  return maps;
}
bool can_start_territory(const MapSelectionContext &selection, bool active) {
  return !active && !selection.manual_selection().empty();
}

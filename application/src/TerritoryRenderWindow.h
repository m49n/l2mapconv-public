#pragma once
#include <territory/Job.h>
class MapSelectionContext;
class ClientSessionContext;
class TerritoryRenderController;
std::vector<std::string> selected_render_maps(const MapSelectionContext &);
bool can_start_territory(const MapSelectionContext &, bool active);
struct TerritoryRenderViewState {
  territory::Settings settings;
  std::filesystem::path output;
  std::string error;
};
void draw_territory_render_window(TerritoryRenderViewState &,
                                  TerritoryRenderController &,
                                  const MapSelectionContext &,
                                  ClientSessionContext &);

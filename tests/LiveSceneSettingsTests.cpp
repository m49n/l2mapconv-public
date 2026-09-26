#include "LiveSceneSettings.h"
#include "TerritoryRenderWindow.h"
#include "TestSupport.h"

auto run_live_scene_settings_tests() -> int {
  int failures{};
  LiveSceneSettings live;
  TerritoryRenderViewState export_view{};
  failures += expect(live.water && live.textures && !live.shadows &&
                         live.sun_azimuth_deg == 315.f &&
                         live.sun_elevation_deg == 40.f,
                     "live defaults enable materials and water but not shadows");
  live.water = false;
  live.textures = false;
  live.shadows = true;
  live.sun_azimuth_deg = 120.f;
  live.sun_elevation_deg = 55.f;
  failures += expect(export_view.settings.water && export_view.settings.textures &&
                         !export_view.settings.shadows &&
                         export_view.settings.sun_azimuth_deg == 315.0 &&
                         export_view.settings.sun_elevation_deg == 40.0,
                     "live changes never alter territory PNG export settings");
  LiveSceneDiagnostics failed;
  failed.error = "fixture depth FBO failure";
  apply_live_shadow_error(live, failed);
  failures += expect(!live.shadows && !live.water && !live.textures &&
                         live.sun_azimuth_deg == 120.f &&
                         live.sun_elevation_deg == 55.f,
                     "shadow failure disables only live shadows");
  return failures;
}

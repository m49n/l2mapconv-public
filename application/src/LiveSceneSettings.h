#pragma once

#include <string>

struct LiveSceneSettings {
  bool water{false};
  bool textures{false};
  bool shadows{false};
  bool culling{true};
  bool wireframe{false};
  bool terrain{true};
  bool static_meshes{true};
  bool csg{true};
  bool passable{false};
  float sun_azimuth_deg{315.f};
  float sun_elevation_deg{40.f};
};

struct LiveSceneDiagnostics {
  int draws{};
  int shadow_map_size{};
  int omitted_casters{};
  int gpu_programs{};
  int gpu_textures{};
  int fallback_textures{};
  int fallback_materials{};
  int singular_recovered{};
  int collapsed_skipped{};
  std::string error;
};

inline void apply_live_shadow_error(LiveSceneSettings &settings,
                                    const LiveSceneDiagnostics &diagnostics) {
  if (settings.shadows && !diagnostics.error.empty()) {
    settings.shadows = false;
  }
}

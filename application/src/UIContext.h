#pragma once

#include "LiveSceneSettings.h"
#include <navmesh/Settings.h>

#include <functional>

struct UIContext {
  struct {
    float speed{3000.0f};
    float mouse_sensitivity{0.002f};
  } camera;

  struct {
    int draws;
    bool textures;
    bool culling;
    bool wireframe;
    bool passable;
    bool terrain;
    bool static_meshes;
    bool csg;
    bool blocking_volumes;
    bool bounding_boxes;
    bool imported_geodata;
    bool generated_geodata;
    LiveSceneSettings live;
    LiveSceneDiagnostics live_diagnostics;

    void set_defaults() {
      culling = true;
      terrain = true;
      static_meshes = true;
      csg = true;
      blocking_volumes = true;
      generated_geodata = true;
    }
  } rendering;

  struct {
    float actor_height;
    float actor_radius;
    float max_walkable_angle;
    float min_walkable_climb;
    float max_walkable_climb;
    float cell_size;
    float cell_height;

    std::function<void()> build_handler;
    bool should_export;
    bool streaming_preview{false};
    bool client_dat{true};
    bool l2j{true}, navmesh{false};
    navmesh::Settings navmesh_settings{};

    void set_defaults() {
      actor_height = 48.0f;
      actor_radius = 16.0f;
      max_walkable_angle = 45.5f;
      min_walkable_climb = 2.0f;
      max_walkable_climb = 16.0f;
      cell_size = 16.0f;
      cell_height = 1.0f;
      navmesh_settings = {};
    }
  } geodata;
};

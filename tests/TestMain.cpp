#include "CameraMotion.h"
#include "ClientFolderPicker.h"
#include "CommandLine.h"
#include "ExecutablePath.h"
#include "MapLoadOptions.h"
#include "SceneStats.h"
#include "SurfaceVisibility.h"
#include "TestSuites.h"
#include "TestSupport.h"
#include "UISettings.h"
#include "UnrealConverters.h"
#include "UnrealLoader.h"

#include <filesystem>
#include <optional>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<decltype(choose_client_root_folder()),
                             std::optional<std::filesystem::path>>);

int run_live_scene_settings_tests();

namespace {

auto near(const glm::vec3 &actual, const glm::vec3 &expected,
          float tolerance = 0.001f) -> bool {
  return glm::length(actual - expected) <= tolerance;
}

} // namespace

auto main(int argc, char **argv) -> int {
  if (argc == 4 && std::string_view{argv[1]} == "--check-map-collisions") {
    return run_static_mesh_collision_tests(argv[2], argv[3]) == 0
               ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (argc == 4 && std::string_view{argv[1]} == "--probe-map-layers") {
    UnrealLoader loader{argv[2]};
    std::cout << "terrain-only\n";
    print_scene_stats(loader.load_map(argv[3], MapLoadOptions::terrain_only()));
    std::cout << "detail-only\n";
    print_scene_stats(loader.load_map(argv[3], MapLoadOptions::detail_only()));
    return EXIT_SUCCESS;
  }

  auto failures = 0;

  failures += expect(true, "test harness accepts successes");
  constexpr auto client_root =
      R"(D:\line\clients\Lineage II - Essence - Samurai Crow - EU-P542\sam)";
  failures += expect(client_root_path(client_root) ==
                         std::filesystem::path{client_root},
                     "client root preserves spaces");
  const auto executable_path = running_executable_path();
  failures += expect(std::filesystem::is_regular_file(executable_path),
                     "running executable path resolves to an existing file");
  failures += expect(executable_path.filename() ==
                         std::filesystem::path{argv[0]}.filename(),
                     "running executable path identifies the current module");
  failures += expect(imgui_ini_filename() == nullptr,
                     "preview never writes automatic ImGui settings");
  failures += expect(primary_uv({}, 0) == glm::vec2{0.0f, 0.0f},
                     "missing UV stream uses zero UV");

  failures += run_map_catalog_tests();
  failures += run_map_residency_tests();
  failures += run_map_load_options_tests();
  failures += run_scene_group_tests();
  failures += run_live_scene_settings_tests();
  failures += run_map_selection_tests();
  failures += run_map_loading_tests();
  failures += run_map_streaming_tests();
  failures += run_system_stack_tests();
  failures += run_imported_geodata_tests();
  failures += run_pts_geodata_tests();
  failures += run_geodata_generation_tests();
  failures += run_recent_clients_tests();
  failures += run_client_startup_tests();
  failures += run_client_session_tests();
  failures += run_desktop_startup_tests();

  unreal::StaticMeshUVStream stream{};
  stream.uvs.push_back({0.25f, 0.75f});
  const std::vector streams{stream};

  failures += expect(primary_uv(streams, 0) == glm::vec2{0.25f, 0.75f},
                     "primary UV is preserved");
  failures += expect(primary_uv(streams, 1) == glm::vec2{0.0f, 0.0f},
                     "short UV stream uses zero UV");

  const glm::vec3 forward{0.0f, 1.0f, 0.0f};
  const glm::vec3 right{1.0f, 0.0f, 0.0f};
  const glm::vec3 up{0.0f, 0.0f, 1.0f};

  failures += expect(near(camera_translation({.forward = true}, forward, right,
                                             up, 0.05f, 1000.0f),
                          {0.0f, -50.0f, 0.0f}),
                     "forward movement uses base speed and frame time");

  const auto diagonal = camera_translation({.forward = true, .right = true},
                                           forward, right, up, 0.05f, 1000.0f);
  failures += expect(std::abs(glm::length(diagonal) - 50.0f) < 0.001f,
                     "diagonal movement is normalized");

  failures += expect(
      near(camera_translation({.up = true}, forward, right, up, 0.05f, 1000.0f),
           {0.0f, 0.0f, 50.0f}),
      "up movement follows the camera up vector");
  failures += expect(near(camera_translation({.down = true}, forward, right, up,
                                             0.05f, 1000.0f),
                          {0.0f, 0.0f, -50.0f}),
                     "down movement opposes the camera up vector");

  failures += expect(near(camera_translation({.forward = true, .fast = true},
                                             forward, right, up, 0.01f, 100.0f),
                          {0.0f, -10.0f, 0.0f}),
                     "fast movement uses the ten-times multiplier");
  failures += expect(near(camera_translation({.forward = true, .slow = true},
                                             forward, right, up, 0.01f, 100.0f),
                          {0.0f, -0.2f, 0.0f}),
                     "slow movement uses the one-fifth multiplier");
  failures += expect(
      near(camera_translation({.forward = true, .fast = true, .slow = true},
                              forward, right, up, 0.01f, 100.0f),
           {0.0f, -2.0f, 0.0f}),
      "fast and slow modifiers compose");

  failures += expect(near(camera_translation({.forward = true}, forward, right,
                                             up, 1.0f, 100.0f),
                          {0.0f, -10.0f, 0.0f}),
                     "frame time is clamped to one tenth of a second");
  failures +=
      expect(near(camera_translation({.forward = true,
                                      .backward = true,
                                      .left = true,
                                      .right = true,
                                      .up = true,
                                      .down = true},
                                     forward, right, up, 0.05f, 1000.0f),
                  {0.0f, 0.0f, 0.0f}),
             "opposing inputs cancel");

  failures += expect(surface_filter({.passable = true}) == SURFACE_PASSABLE,
                     "passable visibility adds only its modifier bit");
  failures += expect(surface_filter({.terrain = true}) == SURFACE_TERRAIN,
                     "terrain visibility adds only the terrain bit");
  failures +=
      expect(surface_filter({.static_meshes = true}) == SURFACE_STATIC_MESH,
             "static mesh visibility adds only the static mesh bit");
  failures += expect(surface_filter({.csg = true}) == SURFACE_CSG,
                     "CSG visibility adds only the CSG bit");
  failures += expect(surface_filter({.blocking_volumes = true}) ==
                         SURFACE_BLOCKING_VOLUME,
                     "blocking volume visibility adds only its own bit");
  failures +=
      expect(surface_filter({.bounding_boxes = true}) == SURFACE_BOUNDING_BOX,
             "bounding box visibility adds only its own bit");
  failures += expect(surface_filter({.imported_geodata = true}) ==
                         SURFACE_IMPORTED_GEODATA,
                     "imported geodata visibility adds only its own bit");
  failures += expect(surface_filter({.generated_geodata = true}) ==
                         SURFACE_GENERATED_GEODATA,
                     "generated geodata visibility adds only its own bit");

  Map scene{};
  const auto add_geometry = [&scene](std::uint64_t type,
                                     std::size_t triangle_count) {
    auto mesh = std::make_shared<EntityMesh>();
    mesh->vertices.resize(3);
    mesh->indices.resize(triangle_count * 3);
    mesh->surfaces.push_back({.type = type,
                              .index_offset = 0,
                              .index_count = triangle_count * 3,
                              .material = {}});
    scene.entities.emplace_back(std::move(mesh));
  };

  add_geometry(SURFACE_TERRAIN, 2);
  add_geometry(SURFACE_STATIC_MESH | SURFACE_PASSABLE, 3);
  add_geometry(SURFACE_CSG, 4);
  add_geometry(SURFACE_BLOCKING_VOLUME, 5);

  auto bounding_box = std::make_shared<EntityMesh>();
  bounding_box->vertices.resize(8);
  bounding_box->indices.resize(36);
  bounding_box->surfaces.push_back(
      {.type = SURFACE_TERRAIN | SURFACE_BOUNDING_BOX,
       .index_offset = 0,
       .index_count = 36,
       .material = {}});
  scene.entities.emplace_back(std::move(bounding_box));

  auto stats = map_geometry_stats(scene);
  failures += expect(stats.terrain.actors == 1 && stats.terrain.vertices == 3 &&
                         stats.terrain.triangles == 2,
                     "terrain statistics exclude bounding boxes");
  failures += expect(stats.static_meshes.actors == 1 &&
                         stats.static_meshes.triangles == 3,
                     "static mesh statistics accept modifier bits");
  failures += expect(stats.csg.actors == 1 && stats.csg.triangles == 4,
                     "CSG statistics count loaded geometry");
  failures += expect(stats.blocking_volumes.actors == 1 &&
                         stats.blocking_volumes.triangles == 5,
                     "blocking volume statistics count loaded geometry");
  failures += expect(has_required_preview_geometry(stats),
                     "complete preview geometry passes verification");

  stats.blocking_volumes.triangles = 0;
  failures += expect(!has_required_preview_geometry(stats),
                     "missing preview geometry fails verification");

  return failures;
}

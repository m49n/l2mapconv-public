#include "CommandLine.h"
#include "CameraMotion.h"
#include "TestSupport.h"
#include "SurfaceVisibility.h"
#include "UnrealConverters.h"

#include <filesystem>
#include <vector>

namespace {

auto near(const glm::vec3 &actual, const glm::vec3 &expected,
          float tolerance = 0.001f) -> bool {
  return glm::length(actual - expected) <= tolerance;
}

} // namespace

auto main() -> int {
  auto failures = 0;

  failures += expect(true, "test harness accepts successes");
  constexpr auto client_root =
      R"(D:\line\clients\Lineage II - Essence - Samurai Crow - EU-P542\sam)";
  failures += expect(client_root_path(client_root) ==
                         std::filesystem::path{client_root},
                     "client root preserves spaces");
  failures += expect(
      executable_directory(
          R"(D:\viewer builds\p542\l2mapconv.exe)") ==
          std::filesystem::path{R"(D:\viewer builds\p542)"},
      "renderer resources resolve beside the executable");
  failures += expect(primary_uv({}, 0) == glm::vec2{0.0f, 0.0f},
                     "missing UV stream uses zero UV");

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

  failures += expect(
      near(camera_translation({.forward = true}, forward, right, up, 0.05f,
                              1000.0f),
           {0.0f, -50.0f, 0.0f}),
      "forward movement uses base speed and frame time");

  const auto diagonal = camera_translation(
      {.forward = true, .right = true}, forward, right, up, 0.05f, 1000.0f);
  failures += expect(std::abs(glm::length(diagonal) - 50.0f) < 0.001f,
                     "diagonal movement is normalized");

  failures += expect(
      near(camera_translation({.up = true}, forward, right, up, 0.05f,
                              1000.0f),
           {0.0f, 0.0f, 50.0f}),
      "up movement follows the camera up vector");
  failures += expect(
      near(camera_translation({.down = true}, forward, right, up, 0.05f,
                              1000.0f),
           {0.0f, 0.0f, -50.0f}),
      "down movement opposes the camera up vector");

  failures += expect(
      near(camera_translation({.forward = true, .fast = true}, forward,
                              right, up, 0.01f, 100.0f),
           {0.0f, -10.0f, 0.0f}),
      "fast movement uses the ten-times multiplier");
  failures += expect(
      near(camera_translation({.forward = true, .slow = true}, forward,
                              right, up, 0.01f, 100.0f),
           {0.0f, -0.2f, 0.0f}),
      "slow movement uses the one-fifth multiplier");
  failures += expect(
      near(camera_translation(
               {.forward = true, .fast = true, .slow = true}, forward, right,
               up, 0.01f, 100.0f),
           {0.0f, -2.0f, 0.0f}),
      "fast and slow modifiers compose");

  failures += expect(
      near(camera_translation({.forward = true}, forward, right, up, 1.0f,
                              100.0f),
           {0.0f, -10.0f, 0.0f}),
      "frame time is clamped to one tenth of a second");
  failures += expect(
      near(camera_translation({.forward = true, .backward = true,
                               .left = true, .right = true,
                               .up = true, .down = true},
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
  failures += expect(surface_filter({.bounding_boxes = true}) ==
                         SURFACE_BOUNDING_BOX,
                     "bounding box visibility adds only its own bit");
  failures += expect(surface_filter({.imported_geodata = true}) ==
                         SURFACE_IMPORTED_GEODATA,
                     "imported geodata visibility adds only its own bit");
  failures += expect(surface_filter({.generated_geodata = true}) ==
                         SURFACE_GENERATED_GEODATA,
                     "generated geodata visibility adds only its own bit");

  return failures;
}

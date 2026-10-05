#include "LiveVisualRenderer.h"
#include "RendererMapSceneSink.h"
#include "UnrealLoader.h"

#include <GL/glew.h>
#include <rendering/EntityRenderer.h>
#include <territory/PngOutput.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

auto flat_terrain() -> Map {
  Map map;
  map.name = "22_22";
  map.bounding_box = geometry::Box{{-4, -4, -1}, {4, 4, 1}};
  auto mesh = std::make_shared<EntityMesh>();
  mesh->bounding_box = map.bounding_box;
  for (const auto position : {glm::vec3{-4, -4, 0}, glm::vec3{4, -4, 0},
                             glm::vec3{4, 4, 0}, glm::vec3{-4, 4, 0}})
    mesh->vertices.push_back({position, {0, 0, 1}, {0, 0}});
  mesh->indices = {0, 1, 2, 0, 2, 3};
  mesh->surfaces.push_back({SURFACE_TERRAIN, 0, 6, {{.85f, .85f, .85f}, {}}});
  map.entities.emplace_back(std::move(mesh));
  return map;
}

auto draw_terrain(RenderingContext &context, LiveVisualRenderer &live)
    -> std::array<unsigned char, 4> {
  glViewport(0, 0, 128, 128);
  glClearColor(.1f, .1f, .1f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  rendering::FrameSettings settings{};
  settings.surface_filter = SURFACE_TERRAIN;
  settings.culling = true;
  int draws{};
  rendering::EntityRenderer{context.context, context.camera}.render(
      context.scene, settings, draws);
  LiveSceneSettings live_settings;
  live_settings.textures = live_settings.water = true;
  LiveSceneDiagnostics diagnostics;
  live.render(context.camera, live_settings, diagnostics);
  std::array<unsigned char, 4> pixel{};
  glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
  return pixel;
}

} // namespace

int run_manual_terrain_renderer_tests() {
  RenderingContext context;
  context.context.framebuffer.size = {128, 128};
  context.camera.set_top_view({0, 0}, 8, -1, 1);
  Renderer renderer{context, L2MAPCONV_TEST_RESOURCES};
  LiveVisualRenderer live{context.context};
  RendererMapSceneSink sink{renderer, live, context,
                            std::filesystem::temp_directory_path() /
                                "l2mapconv-absent-geodata"};
  const auto terrain = flat_terrain();
  const auto group = sink.upload({22, 22}, MapLayer::Terrain, {terrain, {}});
  const auto pixel = draw_terrain(context, live);
  const bool visible = pixel[0] > 100 && pixel[1] > 100 && pixel[2] > 100;
  if (!visible)
    std::cerr << "FAIL: selected terrain remains visible in top view with Live Textures/Water enabled, pixel="
              << int(pixel[0]) << ',' << int(pixel[1]) << ',' << int(pixel[2]) << '\n';
  auto detail = std::make_shared<territory::VisualScene>();
  detail->bounds = {-4, -4, 4, 4, -1, 1};
  territory::VisualMesh visual_mesh;
  for (const auto &vertex : terrain.entities.front().mesh->vertices) {
    territory::VisualVertex converted;
    converted.position = vertex.position;
    converted.normal = vertex.normal;
    visual_mesh.vertices.push_back(converted);
  }
  visual_mesh.indices = {0, 1, 2, 0, 2, 3};
  detail->meshes.push_back(std::move(visual_mesh));
  detail->terrain_mesh = 0;
  Map overlay;
  overlay.name = "22_22";
  overlay.bounding_box = terrain.bounding_box;
  const auto detail_group = sink.upload({22, 22}, MapLayer::Detail,
                                         {overlay, std::move(detail)});
  sink.set_visible(group, false);
  draw_terrain(context, live);
  sink.remove(detail_group);
  sink.set_visible(group, true);
  const auto restored = draw_terrain(context, live);
  const bool restored_visible = restored[0] > 100 && restored[1] > 100 &&
                                 restored[2] > 100;
  if (!restored_visible)
    std::cerr << "FAIL: removing detail restores the selected terrain's top-view pixels\n";
  sink.remove(group);
  return static_cast<int>(!visible) + static_cast<int>(!restored_visible);
}

int inspect_manual_terrain(int argc, char **argv) {
  if (argc != 6)
    throw std::runtime_error{"Expected client map output width"};
  const std::filesystem::path output{argv[4]};
  if (std::filesystem::exists(output))
    throw std::runtime_error{"Manual terrain probe output must be new"};
  UnrealLoader loader{argv[2]};
  const auto map = loader.load_map(argv[3], MapLoadOptions::terrain_only());
  const auto lo = map.bounding_box.min(), hi = map.bounding_box.max();
  RenderingContext context;
  context.context.framebuffer.size = {128, 128};
  context.camera.set_top_view({76905, 150907}, std::stof(argv[5]), -32768, 32768);
  Renderer renderer{context, L2MAPCONV_TEST_RESOURCES};
  LiveVisualRenderer live{context.context};
  RendererMapSceneSink sink{renderer, live, context};
  sink.upload({22, 22}, MapLayer::Terrain, {map, {}});
  const auto center = draw_terrain(context, live);
  std::vector<unsigned char> pixels(128 * 128 * 3), flipped(pixels.size());
  glReadPixels(0, 0, 128, 128, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
  std::size_t covered{};
  for (std::size_t i = 0; i < pixels.size(); i += 3)
    covered += pixels[i] > 40 || pixels[i+1] > 40 || pixels[i+2] > 40;
  for (int row = 0; row < 128; ++row)
    std::copy_n(pixels.data() + (127-row)*128*3, 128*3,
                flipped.data() + row*128*3);
  territory::PngOutput png{output, 128, 128};
  png.rows(0, 128, 128, flipped);
  png.finish({});
  std::cout << "terrain entities=" << map.entities.size()
            << " bounds=" << lo.x << ',' << lo.y << ',' << lo.z << ".."
            << hi.x << ',' << hi.y << ',' << hi.z
            << " pixel=" << int(center[0]) << ',' << int(center[1]) << ','
            << int(center[2]) << " covered=" << covered << "/16384\n";
  return 0;
}

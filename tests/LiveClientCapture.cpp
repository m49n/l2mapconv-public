#include "LiveClientCapture.h"
#include "LiveSceneSettings.h"
#include "LiveVisualRenderer.h"
#include <GL/glew.h>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <set>
#include <territory/PathIO.h>
#include <territory/PngOutput.h>
#include <territory/VisualSceneLoader.h>
#include <unreal/Actor.h>
#include <unreal/ArchiveLoader.h>

// Reproducible GPU captures without operating the user's open application.
int capture_live_client(rendering::Context &context, int argc, char **argv) {
  if (argc != 10)
    throw std::runtime_error{"Expected client, map, output, x y z yaw pitch"};
  const std::filesystem::path root{argv[2]}, output{argv[4]};
  const std::string map{argv[3]};
  std::filesystem::create_directories(output);
  territory::VisualSceneLoader loader{root};
  auto scene = loader.load(map);
  territory::Json nearby = territory::Json::array();
  const glm::vec2 target{std::stof(argv[5]), std::stof(argv[6])};
  for (const auto &draw : scene.draws) {
    glm::vec3 lo{1e30f}, hi{-1e30f};
    const auto &mesh = scene.meshes[draw.mesh];
    for (std::size_t i = draw.first_index;
         i < draw.first_index + draw.index_count; ++i) {
      const auto p =
          glm::vec3(draw.transform *
                    glm::vec4(mesh.vertices[mesh.indices[i]].position, 1));
      lo = glm::min(lo, p);
      hi = glm::max(hi, p);
    }
    if (lo.x > target.x + 4000 || hi.x < target.x - 4000 ||
        lo.y > target.y + 4000 || hi.y < target.y - 4000)
      continue;
    nearby.push_back(
        {{"draw", draw.source},
         {"material", scene.library.materials[draw.material].source},
         {"min", {lo.x, lo.y, lo.z}},
         {"max", {hi.x, hi.y, hi.z}}});
  }
  territory::write_json_atomic(output / "nearby.json", nearby, false);
  territory::write_json_atomic(output / "materials.json",
                               territory::material_inventory(scene), false);

  unreal::ArchiveLoader archives{root,
                                 {unreal::SearchConfig{"maps", "unr"},
                                  unreal::SearchConfig{"StaticMeshes", "usx"},
                                  unreal::SearchConfig{"Textures", "utx"},
                                  unreal::SearchConfig{"SysTextures", "utx"}}};
  auto *archive = archives.load_archive(map);
  std::set<std::string> seen;
  territory::Json streams = territory::Json::array();
  for (auto &entry : archive->export_map) {
    if (entry.class_name != "StaticMeshActor")
      continue;
    auto actor = std::dynamic_pointer_cast<unreal::Actor>(
        archive->object_loader.export_object(entry));
    if (!actor || !actor->static_mesh.has_reference())
      continue;
    auto mesh = actor->static_mesh.as<unreal::StaticMesh>();
    if (!mesh)
      continue;
    const auto ref = mesh->asset_reference();
    const auto name = ref.package + "." + ref.object_path;
    if (!seen.insert(name).second)
      continue;
    territory::Json uv = territory::Json::array();
    for (const auto &stream : mesh->uv_stream) {
      territory::Json samples = territory::Json::array();
      for (std::size_t i = 0; i < std::min<std::size_t>(stream.uvs.size(), 4);
           ++i)
        samples.push_back({stream.uvs[i].u, stream.uvs[i].v});
      uv.push_back({{"coordinate_index", stream.coordinate_index},
                    {"count", stream.uvs.size()},
                    {"samples", samples}});
    }
    streams.push_back({{"mesh", name}, {"uv_streams", uv}});
  }
  territory::write_json_atomic(output / "uv-streams.json", streams, false);
  context.framebuffer.size = {1024, 768};
  rendering::Camera camera{
      context,
      60.f,
      .1f,
      {std::stof(argv[5]), std::stof(argv[6]), std::stof(argv[7])}};
  camera.rotate(glm::radians(std::stof(argv[8])), camera.up());
  camera.rotate(glm::radians(std::stof(argv[9])), camera.right());
  const auto forward = camera.forward();
  std::cout << "camera forward=" << forward.x << ',' << forward.y << ','
            << forward.z << '\n';
  LiveVisualRenderer renderer{context};
  renderer.upload(1, scene);
  for (int variant = 0; variant < 4; ++variant) {
    LiveSceneSettings settings;
    settings.textures = variant != 0;
    settings.water = variant != 0;
    settings.shadows = variant >= 2;
    if (variant == 3) {
      settings.sun_azimuth_deg = 135;
      settings.sun_elevation_deg = 70;
    }
    LiveSceneDiagnostics diagnostics;
    glViewport(0, 0, 1024, 768);
    glClearColor(.1f, .1f, .1f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(camera, settings, diagnostics);
    if (!diagnostics.error.empty())
      throw std::runtime_error{diagnostics.error};
    std::vector<std::uint8_t> pixels(1024 * 768 * 3), flipped(pixels.size());
    glReadPixels(0, 0, 1024, 768, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    for (int row = 0; row < 768; ++row)
      std::copy_n(pixels.data() + (767 - row) * 1024 * 3, 1024 * 3,
                  flipped.data() + row * 1024 * 3);
    const char *names[] = {"geometry.png", "textures.png", "sun315-40.png",
                           "sun135-70.png"};
    territory::PngOutput png{output / names[variant], 1024, 768};
    png.rows(0, 1024, 768, flipped);
    png.finish({});
    std::cout << names[variant] << " draws=" << diagnostics.draws << '\n';
  }
  return 0;
}

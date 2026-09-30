#include "LiveSceneSettings.h"
#include "LiveVisualRenderer.h"
#include "LiveClientCapture.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <territory/VisualSceneLoader.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

struct HiddenWindow {
  GLFWwindow *window{};
  HiddenWindow(int width = 128, int height = 128) {
    if (!glfwInit()) throw std::runtime_error{"GLFW initialization failed"};
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(width, height, "Live visual tests", nullptr, nullptr);
    if (!window) throw std::runtime_error{"Hidden OpenGL window unavailable"};
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) throw std::runtime_error{"GLEW initialization failed"};
    while (glGetError() != GL_NO_ERROR) {}
  }
  ~HiddenWindow() {
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
  }
};

auto quad_scene(bool water) -> territory::VisualScene {
  using namespace territory;
  VisualScene scene;
  scene.bounds = {-4, 9, 4, 11, -4, 4};
  VisualMesh mesh;
  for (const auto &position : {glm::vec3{-4, 10, -4}, glm::vec3{4, 10, -4},
                               glm::vec3{4, 10, 4}, glm::vec3{-4, 10, 4}}) {
    VisualVertex vertex;
    vertex.position = position;
    vertex.normal = {0, -1, 0};
    vertex.uv[0] = {(position.x + 4) / 8, (position.z + 4) / 8};
    mesh.vertices.push_back(vertex);
  }
  mesh.indices = {0, 1, 2, 0, 2, 3};
  scene.meshes.push_back(std::move(mesh));
  TextureData texture;
  texture.source = "red fixture";
  texture.width = texture.height = 2;
  texture.bytes = {255, 0, 0, 255, 255, 0, 0, 255,
                   255, 0, 0, 255, 255, 0, 0, 255};
  scene.library.textures.push_back(std::move(texture));
  RenderMaterial material;
  material.source = "fixture";
  material.unlit = true;
  material.two_sided = true;
  Node sample;
  sample.op = Op::Sample;
  sample.texture = 0;
  material.nodes.push_back(sample);
  scene.library.materials.push_back(std::move(material));
  scene.draws.push_back({0, 0, 0, 6, glm::mat4{1}, water, "fixture quad"});
  return scene;
}

auto pixel() -> std::array<std::uint8_t, 4> {
  std::array<std::uint8_t, 4> value{};
  glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, value.data());
  return value;
}

auto pixel_at(const rendering::Camera &camera, glm::vec3 world)
    -> std::array<std::uint8_t, 4> {
  const auto clip = camera.projection_matrix() * camera.view_matrix() *
                    glm::vec4{world, 1.f};
  const auto ndc = glm::vec3{clip} / clip.w;
  const auto x = static_cast<int>((ndc.x * .5f + .5f) * 128);
  const auto y = static_cast<int>((ndc.y * .5f + .5f) * 128);
  std::array<std::uint8_t, 4> result{};
  glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
  return result;
}

auto shadow_scene() -> territory::VisualScene {
  using namespace territory;
  VisualScene scene;
  scene.bounds = {-12, -2, 12, 22, 0, 6};
  auto add = [&](float left, float front, float right, float back, float z,
                 glm::vec4 color) {
    VisualMesh mesh;
    for (const auto &xy : {glm::vec2{left, front}, glm::vec2{right, front},
                           glm::vec2{right, back}, glm::vec2{left, back}}) {
      VisualVertex vertex;
      vertex.position = {xy.x, xy.y, z};
      vertex.normal = {0, 0, 1};
      vertex.uv[0] = {(xy.x - left) / (right - left),
                      (xy.y - front) / (back - front)};
      mesh.vertices.push_back(vertex);
    }
    mesh.indices = {0, 1, 2, 0, 2, 3};
    RenderMaterial material;
    material.source = "shadow fixture";
    material.unlit = true;
    material.two_sided = true;
    Node node;
    node.value = color;
    material.nodes.push_back(node);
    scene.draws.push_back({scene.meshes.size(), scene.library.materials.size(),
                           0, 6, glm::mat4{1}, false, "shadow quad"});
    scene.meshes.push_back(std::move(mesh));
    scene.library.materials.push_back(std::move(material));
  };
  add(-12, -2, 12, 22, 0, {1, 1, 1, 1});
  add(-2, 8, 2, 12, 5, {1, 0, 0, 1});
  scene.draws.back().source = "fixture.BSP:0";
  return scene;
}

auto draw(LiveVisualRenderer &renderer, const rendering::Camera &camera,
          const LiveSceneSettings &settings) -> std::array<std::uint8_t, 4> {
  glViewport(0, 0, 128, 128);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  LiveSceneDiagnostics diagnostics;
  renderer.render(camera, settings, diagnostics);
  if (!diagnostics.error.empty())
    throw std::runtime_error{diagnostics.error};
  return pixel();
}

auto check(bool pass, const char *name) -> int {
  if (!pass) std::cerr << "FAIL: " << name << '\n';
  return !pass;
}

} // namespace

int main(int argc, char **argv) {
  try {
    const bool capture = argc > 1 && std::string_view{argv[1]} == "--capture-client";
    HiddenWindow window{capture ? 1024 : 128, capture ? 768 : 128};
    rendering::Context context{};
    if (capture) return capture_live_client(context, argc, argv);
    context.framebuffer.size = {128, 128};
    if (argc == 4 && std::string_view{argv[1]} == "--client-smoke") {
      territory::VisualSceneLoader loader{std::filesystem::path{argv[2]}};
      auto scene = loader.load(argv[3]);
      std::cout << argv[3] << " meshes=" << scene.meshes.size()
                << " draws=" << scene.draws.size()
                << " textures=" << scene.library.textures.size()
                << " water=" << std::count_if(scene.draws.begin(), scene.draws.end(),
                                               [](const auto &draw) { return draw.water; })
                << '\n';
      LiveVisualRenderer preview{context};
      const auto upload_start = std::chrono::steady_clock::now();
      preview.upload(1, scene);
      const auto upload_ms = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - upload_start).count();
      const auto center_x = static_cast<float>((scene.bounds.min_x + scene.bounds.max_x) / 2);
      const auto center_y = static_cast<float>((scene.bounds.min_y + scene.bounds.max_y) / 2);
      rendering::Camera fly{context, 60.f, 0.1f,
                            {center_x, center_y, static_cast<float>(scene.bounds.max_z + 8192)}};
      fly.rotate(glm::radians(-90.f), {1, 0, 0});
      LiveSceneSettings settings;
      settings.textures = settings.water = true;
      LiveSceneDiagnostics diagnostics;
      glViewport(0, 0, 128, 128);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      const auto color_start = std::chrono::steady_clock::now();
      preview.render(fly, settings, diagnostics);
      glFinish();
      const auto color_ms = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - color_start).count();
      if (!diagnostics.error.empty() || diagnostics.draws == 0)
        throw std::runtime_error{"Live client color pass failed: " + diagnostics.error};
      std::vector<std::uint8_t> textured_frame(128 * 128 * 4);
      glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE,
                   textured_frame.data());
      settings.textures = false;
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      preview.render(fly, settings, diagnostics);
      std::vector<std::uint8_t> flat_frame(textured_frame.size());
      glReadPixels(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE,
                   flat_frame.data());
      if (textured_frame == flat_frame)
        throw std::runtime_error{"Real client textures do not affect live RGB"};
      settings.textures = true;
      settings.shadows = true;
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      const auto shadow_start = std::chrono::steady_clock::now();
      preview.render(fly, settings, diagnostics);
      glFinish();
      const auto shadow_ms = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - shadow_start).count();
      if (!diagnostics.error.empty() || diagnostics.shadow_map_size != 2048)
        throw std::runtime_error{"Live client shadow pass failed: " + diagnostics.error};
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      const auto steady_start = std::chrono::steady_clock::now();
      preview.render(fly, settings, diagnostics);
      glFinish();
      const auto steady_ms = std::chrono::duration<double, std::milli>(
          std::chrono::steady_clock::now() - steady_start).count();
      std::cout << "live draws=" << diagnostics.draws
                << " shadow=" << diagnostics.shadow_map_size
                << " omitted=" << diagnostics.omitted_casters << '\n';
      std::cout << "color_ms=" << color_ms << " first_shadow_ms="
                << shadow_ms << " steady_shadow_ms=" << steady_ms
                << " upload_ms=" << upload_ms << '\n';
      settings.shadows = false;
      fly.set_position({center_x, center_y, 4000.f});
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      const auto near_start = std::chrono::steady_clock::now();
      preview.render(fly, settings, diagnostics);
      glFinish();
      std::cout << "near_draws=" << diagnostics.draws << " near_ms="
                << std::chrono::duration<double, std::milli>(
                       std::chrono::steady_clock::now() - near_start).count()
                << '\n';
      preview.remove(1);
      return 0;
    }
    if (argc != 1) throw std::runtime_error{"Invalid live visual test arguments"};
    rendering::Camera camera{context, 60.f, 0.1f, {0, 0, 0}};
    LiveVisualRenderer renderer{context};
    LiveSceneSettings settings;
    settings.textures = settings.water = true;
    int failures = 0;

    context.shader.program = 7;
    context.mesh.vao = 8;
    context.texture.texture = 9;
    renderer.upload(1, quad_scene(false));
    failures += check(context.shader.program == 0 && context.mesh.vao == 0 &&
                          context.texture.texture == 0,
                      "live upload invalidates legacy GL binding caches");
    context.shader.program = 7;
    context.mesh.vao = 8;
    context.texture.texture = 9;
    const auto textured = draw(renderer, camera, settings);
    GLint active_texture{}, current_program{}, vertex_array{};
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
    glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vertex_array);
    failures += check(context.shader.program == 0 && context.mesh.vao == 0 &&
                          context.texture.texture == 0 &&
                          active_texture == GL_TEXTURE0 &&
                          current_program == 0 && vertex_array == 0,
                      "live render restores legacy-compatible GL bindings");
    settings.static_meshes = false;
    failures += check(draw(renderer, camera, settings)[0] == 0,
                      "Static Meshes switch hides live actor geometry");
    settings.static_meshes = true;
    settings.textures = false;
    const auto flat = draw(renderer, camera, settings);
    failures += check(textured[0] > textured[1] + 80 &&
                          flat[0] > flat[1] + 60 && flat[1] > textured[1],
                      "Textures toggle changes live RGB without reupload");
    LiveSceneDiagnostics populated;
    renderer.render(camera, settings, populated);
    failures += check(populated.gpu_programs > 0,
                      "resident visual group owns compiled shader resources");
    renderer.upload(13, quad_scene(false));
    renderer.render(camera, settings, populated);
    failures += check(populated.gpu_textures == 1,
                      "identical texture is shared by two resident map groups");
    renderer.remove(1);
    renderer.render(camera, settings, populated);
    failures += check(populated.gpu_textures == 1,
                      "shared texture survives unloading one of its map groups");
    renderer.remove(13);
    failures += check(draw(renderer, camera, settings)[0] == 0,
                      "removing resident group removes its live pixels");
    LiveSceneDiagnostics released;
    renderer.render(camera, settings, released);
    failures += check(released.gpu_programs == 0 &&
                          released.gpu_textures == 0,
                      "unloading the last visual group releases shaders and textures");

    renderer.upload(2, quad_scene(true));
    settings.water = false;
    failures += check(draw(renderer, camera, settings)[0] == 0,
                      "Water=false hides only water surfaces");
    settings.water = true;
    failures += check(draw(renderer, camera, settings)[0] > 0,
                      "Water=true shows verified water geometry");
    renderer.remove(2);

    auto terrain = quad_scene(false);
    terrain.terrain_mesh = 0;
    terrain.draws.clear();
    renderer.upload(14, terrain);
    settings.terrain = false;
    failures += check(draw(renderer, camera, settings)[0] == 0,
                      "Terrain switch hides live terrain base");
    settings.terrain = true;
    failures += check(draw(renderer, camera, settings)[0] > 0,
                      "Terrain switch restores live terrain base");
    renderer.remove(14);

    auto csg = quad_scene(false);
    csg.draws.front().source = "fixture.BSP:0";
    renderer.upload(15, csg);
    settings.csg = false;
    failures += check(draw(renderer, camera, settings)[0] == 0,
                      "CSG switch hides live BSP surfaces");
    settings.csg = true;
    failures += check(draw(renderer, camera, settings)[0] > 0,
                      "CSG switch restores live BSP surfaces");
    const auto csg_flat = draw(renderer, camera, settings);
    failures += check(csg_flat[0] > csg_flat[2] + 60 && csg_flat[1] > csg_flat[2] + 60,
                      "untextured BSP retains the original yellow geometry palette");
    renderer.remove(15);

    auto transparent_flat = quad_scene(false);
    transparent_flat.library.materials[0].blend = territory::Blend::Alpha;
    transparent_flat.library.materials[0].depth_write = false;
    for (std::size_t i = 3; i < transparent_flat.library.textures[0].bytes.size(); i += 4)
      transparent_flat.library.textures[0].bytes[i] = 0;
    renderer.upload(30, transparent_flat);
    const auto solid_flat = draw(renderer, camera, settings);
    failures += check(solid_flat[0] > 180 && solid_flat[1] > 60,
                      "geometry preview ignores material opacity and remains solid colored geometry");
    renderer.remove(30);
    transparent_flat.library.materials[0].blend = territory::Blend::Masked;
    transparent_flat.library.materials[0].alpha_test = true;
    renderer.upload(36, transparent_flat);
    failures += check(draw(renderer, camera, settings)[0] > 180,
                      "untextured geometry does not disappear through material alpha-test holes");
    renderer.remove(36);

    auto passable_flat = quad_scene(false);
    passable_flat.draws[0].passable = true;
    renderer.upload(34, passable_flat);
    settings.passable = false;
    failures += check(draw(renderer, camera, settings)[0] == 0,
                      "Passable=false hides noncolliding geometry in the flat preview");
    settings.passable = true;
    const auto green_flat = draw(renderer, camera, settings);
    failures += check(green_flat[1] > green_flat[0] + 60,
                      "Passable geometry retains the original green palette");
    renderer.remove(34);
    settings.passable = false;

    auto back_wall = quad_scene(false);
    back_wall.library.materials[0].two_sided = false;
    renderer.upload(33, back_wall);
    settings.textures = true;
    settings.culling = true;
    failures += check(draw(renderer, camera, settings)[0] > 200,
                      "left-handed flight camera keeps the wall's front face");
    renderer.remove(33);
    back_wall.draws[0].transform = glm::scale(glm::mat4{1}, {-1.f, 1.f, 1.f});
    renderer.upload(37, back_wall);
    failures += check(draw(renderer, camera, settings)[0] > 200,
                      "mirrored actor preserves its front face in the left-handed flight camera");
    renderer.remove(37);
    back_wall.draws[0].transform = glm::mat4{1};
    for (std::size_t i = 0; i < back_wall.meshes[0].indices.size(); i += 3)
      std::swap(back_wall.meshes[0].indices[i], back_wall.meshes[0].indices[i + 2]);
    renderer.upload(31, back_wall);
    settings.textures = true;
    settings.culling = true;
    failures += check(draw(renderer, camera, settings)[0] == 0,
                      "Culling=true removes the single-sided wall's back face");
    settings.culling = false;
    failures += check(draw(renderer, camera, settings)[0] > 200,
                      "Culling=false reveals back faces for free-flight inspection");
    renderer.remove(31);
    settings.culling = true;

    auto lit_wall = quad_scene(false);
    lit_wall.library.materials[0].unlit = false;
    renderer.upload(32, lit_wall);
    settings.shadows = true;
    settings.sun_azimuth_deg = 315;
    settings.sun_elevation_deg = 15;
    const auto low_sun = draw(renderer, camera, settings);
    settings.sun_elevation_deg = 80;
    const auto high_sun = draw(renderer, camera, settings);
    settings.sun_elevation_deg = 15;
    settings.sun_azimuth_deg = 135;
    const auto reversed_sun = draw(renderer, camera, settings);
    failures += check(low_sun[0] > high_sun[0] + 15,
                      "Sun elevation changes live surface illumination, not only shadow projection");
    failures += check(low_sun[0] > reversed_sun[0] + 15,
                      "Sun direction changes live surface illumination");
    renderer.remove(32);
    settings.shadows = false;

    auto middle_gray = quad_scene(false);
    middle_gray.library.textures[0].source = "sRGB middle gray";
    for (std::size_t i = 0; i < middle_gray.library.textures[0].bytes.size(); i += 4)
      for (int channel = 0; channel < 3; ++channel)
        middle_gray.library.textures[0].bytes[i + channel] = 128;
    renderer.upload(35, middle_gray);
    const auto gray_pixel = draw(renderer, camera, settings);
    failures += check(gray_pixel[0] >= 124 && gray_pixel[0] <= 132,
                      "sRGB textures preserve their brightness on the live window's linear framebuffer");
    renderer.remove(35);

    auto near_alpha = quad_scene(false);
    near_alpha.library.materials[0].blend = territory::Blend::Alpha;
    near_alpha.library.materials[0].depth_write = false;
    for (std::size_t i = 3; i < near_alpha.library.textures[0].bytes.size(); i += 4)
      near_alpha.library.textures[0].bytes[i] = 128;
    auto far_opaque = quad_scene(false);
    far_opaque.library.textures[0].source = "blue fixture";
    for (std::size_t i = 0; i < far_opaque.library.textures[0].bytes.size(); i += 4) {
      far_opaque.library.textures[0].bytes[i] = 0;
      far_opaque.library.textures[0].bytes[i + 2] = 255;
    }
    far_opaque.draws[0].transform = glm::translate(glm::mat4{1}, {0.f, 2.f, 0.f});
    far_opaque.bounds = {-4, 11, 4, 13, -4, 4};
    settings.textures = true;
    renderer.upload(20, near_alpha);
    renderer.upload(21, far_opaque);
    const auto layered = draw(renderer, camera, settings);
    failures += check(layered[0] > 60 && layered[2] > 60,
                      "near transparent surface blends over later group's opaque surface");
    renderer.remove(20);
    renderer.remove(21);

    auto unavailable = quad_scene(false);
    unavailable.library.textures.front().width = 0;
    renderer.upload(8, unavailable);
    LiveSceneDiagnostics fallback_info;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(camera, settings, fallback_info);
    const auto fallback_pixel = pixel();
    failures += check(fallback_info.fallback_textures == 1 &&
                          fallback_pixel[0] > 0 &&
                          fallback_pixel[0] < 240,
                      "unavailable texture keeps a neutral live surface");
    renderer.remove(8);

    GLint max_samplers{};
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_samplers);
    auto over_budget = quad_scene(false);
    over_budget.library.materials.front().nodes.clear();
    for (int i = 0; i < max_samplers + 1; ++i) {
      auto texture = over_budget.library.textures.front();
      texture.source = "over-budget-" + std::to_string(i);
      over_budget.library.textures.push_back(texture);
      territory::Node sample;
      sample.op = territory::Op::Sample;
      sample.texture = static_cast<std::uint32_t>(i + 1);
      over_budget.library.materials.front().nodes.push_back(sample);
    }
    over_budget.library.materials.front().root =
        over_budget.library.materials.front().nodes.size() - 1;
    renderer.upload(16, over_budget);
    LiveSceneDiagnostics budget_info;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(camera, settings, budget_info);
    failures += check(budget_info.error.empty() &&
                          budget_info.fallback_materials == 1 &&
                          pixel()[0] > 0,
                      "over-budget material stays visible with neutral fallback");
    renderer.remove(16);

    auto no_depth = quad_scene(false);
    no_depth.library.materials.front().depth_test = false;
    renderer.upload(17, no_depth);
    draw(renderer, camera, settings);
    failures += check(glIsEnabled(GL_DEPTH_TEST) == GL_TRUE,
                      "live material restores depth testing for legacy overlays");
    renderer.remove(17);

    auto culling_scene = quad_scene(false);
    auto behind = culling_scene.draws.front();
    behind.transform = glm::translate(glm::mat4{1}, glm::vec3{0, -20, 0});
    culling_scene.draws.push_back(behind);
    renderer.upload(5, culling_scene);
    LiveSceneDiagnostics culling_info;
    settings.culling = true;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(camera, settings, culling_info);
    failures += check(culling_info.draws == 1,
                      "live culling skips off-camera visual draws");
    settings.culling = false;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(camera, settings, culling_info);
    failures += check(culling_info.draws == 2,
                      "disabling culling restores both resident draws");
    renderer.remove(5);
    settings.culling = true;

    rendering::Camera angled{context, 60.f, 0.1f, {0, -10, 10}};
    angled.rotate(glm::radians(-30.f), {1, 0, 0});
    renderer.upload(3, shadow_scene());
    settings.textures = true;
    settings.shadows = false;
    draw(renderer, angled, settings);
    const auto southeast_unshadowed = pixel_at(angled, {4, 14, 0});
    settings.shadows = true;
    settings.sun_azimuth_deg = 315.f;
    settings.sun_elevation_deg = 40.f;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    LiveSceneDiagnostics shadow_info;
    renderer.render(angled, settings, shadow_info);
    const auto southeast_shadowed = pixel_at(angled, {4, 14, 0});
    failures += check(shadow_info.error.empty() &&
                          shadow_info.shadow_map_size == 2048 &&
                          southeast_shadowed[0] + 25 < southeast_unshadowed[0],
                      "northwest sun casts a real depth shadow southeast");
    settings.csg = false;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(angled, settings, shadow_info);
    failures += check(pixel_at(angled, {4, 14, 0})[0] >
                          southeast_shadowed[0] + 20,
                      "hidden CSG caster does not darken visible terrain");
    settings.csg = true;
    settings.sun_azimuth_deg = 135.f;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(angled, settings, shadow_info);
    const auto southeast_reversed = pixel_at(angled, {4, 14, 0});
    failures += check(southeast_reversed[0] > southeast_shadowed[0] + 20,
                      "reversing the sun moves the cast shadow");
    settings.shadows = false;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(angled, settings, shadow_info);
    failures += check(shadow_info.shadow_map_size == 0 &&
                          pixel_at(angled, {4, 14, 0})[0] ==
                              southeast_unshadowed[0],
                      "disabling shadows restores unchanged color output");
    renderer.remove(3);

    auto masked_scene = shadow_scene();
    territory::TextureData cutout;
    cutout.source = "checker alpha";
    cutout.width = 2;
    cutout.height = 1;
    cutout.bytes = {255, 0, 0, 255, 255, 0, 0, 0};
    masked_scene.library.textures.push_back(cutout);
    auto &roof = masked_scene.library.materials.back();
    roof.blend = territory::Blend::Masked;
    roof.alpha_test = true;
    roof.alpha_ref = .5f;
    roof.nodes.front().op = territory::Op::Sample;
    roof.nodes.front().texture = 0;
    renderer.upload(6, masked_scene);
    settings.shadows = true;
    settings.sun_azimuth_deg = 315.f;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(angled, settings, shadow_info);
    const auto opaque_shadow = pixel_at(angled, {3, 14, 0});
    const auto hole_shadow = pixel_at(angled, {5, 14, 0});
    failures += check(opaque_shadow[0] + 20 < hole_shadow[0],
                      "masked roof casts only from opaque texels");
    settings.textures = false;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(angled, settings, shadow_info);
    failures += check(pixel_at(angled, {3, 14, 0})[0] + 20 <
                          pixel_at(angled, {5, 14, 0})[0],
                      "Textures=false retains masked coverage in the shadow pass");
    settings.textures = true;
    renderer.remove(6);

    auto uncertain_scene = shadow_scene();
    uncertain_scene.library.materials.back().shadow_coverage_reliable = false;
    renderer.upload(7, uncertain_scene);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(angled, settings, shadow_info);
    failures += check(shadow_info.omitted_casters == 1 &&
                          pixel_at(angled, {4, 14, 0})[0] >
                              southeast_shadowed[0] + 20,
                      "unreliable alpha coverage omits the solid caster");
    renderer.remove(7);

    auto receiver = shadow_scene();
    receiver.draws.resize(1);
    receiver.bounds = {-12, -2, 12, 22, 0, 0};
    auto exterior_caster = shadow_scene();
    exterior_caster.draws.erase(exterior_caster.draws.begin());
    exterior_caster.bounds = {-2, 8, 2, 12, 5, 5};
    renderer.upload(9, receiver);
    renderer.upload(10, exterior_caster);
    rendering::Camera receiver_camera{context, 30.f, .1f, {4, 14, 8}};
    receiver_camera.rotate(glm::radians(-90.f), {1, 0, 0});
    settings.shadows = false;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(receiver_camera, settings, shadow_info);
    const auto no_exterior_shadow = pixel_at(receiver_camera, {4, 14, 0});
    settings.shadows = true;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(receiver_camera, settings, shadow_info);
    const auto exterior_shadow = pixel_at(receiver_camera, {4, 14, 0});
    failures += check(exterior_shadow[0] + 25 < no_exterior_shadow[0],
                      "loaded neighbor casts onto receiver despite being outside camera frustum");
    renderer.remove(10);
    renderer.remove(9);

    auto singular = shadow_scene();
    singular.draws.resize(1);
    singular.draws.front().transform =
        glm::scale(glm::mat4{1}, glm::vec3{1, 1, 0});
    singular.library.materials.front().unlit = false;
    renderer.upload(11, singular);
    settings.shadows = false;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(angled, settings, shadow_info);
    failures += check(shadow_info.singular_recovered == 1 &&
                          pixel_at(angled, {4, 14, 0})[0] > 0,
                      "rank-two surface recovers face normals without vanishing");
    renderer.remove(11);

    auto collapsed = quad_scene(false);
    for (int axis = 0; axis < 3; ++axis)
      collapsed.draws.front().transform[axis] = glm::vec4{0};
    renderer.upload(12, collapsed);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer.render(camera, settings, shadow_info);
    failures += check(shadow_info.draws == 0 &&
                          shadow_info.collapsed_skipped == 1,
                      "point-collapsed visual is skipped without a draw call");
    renderer.remove(12);

    LiveVisualRenderer limited{context, 64};
    limited.upload(4, shadow_scene());
    settings.shadows = true;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    limited.render(angled, settings, shadow_info);
    failures += check(!shadow_info.error.empty() &&
                          pixel_at(angled, {4, 14, 0})[0] > 0,
                      "shadow capacity failure reports error and keeps color scene");
    limited.remove(4);
    failures += check(glGetError() == GL_NO_ERROR,
                      "live and legacy-compatible passes leave no GL error");
    return failures;
  } catch (const std::exception &error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}

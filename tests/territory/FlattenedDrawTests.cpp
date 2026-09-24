#include "TestSupport.h"
#include <algorithm>
#include <cmath>
#include <territory/TerritoryRenderer.h>

namespace {
using namespace territory;
VisualScene plane_fixture() {
  VisualScene scene;
  scene.bounds = {65496, 131032, 65576, 131112, -32, 32};
  VisualMesh mesh;
  // Plane z = .25*x + .5*y + 3, expressed relative to (65536,131072).
  const glm::vec3 positions[] = {{65516, 131052, -12},
                                 {65556, 131052, -2},
                                 {65556, 131092, 18},
                                 {65516, 131092, 8}};
  const glm::vec2 uv[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  for (int i = 0; i < 4; ++i) {
    VisualVertex v;
    v.position = positions[i];
    v.normal = glm::normalize(glm::vec3(-.25f, -.5f, 1));
    v.uv[0] = uv[i];
    v.color = {.5f + .125f * i, 1.f, .75f, .75f};
    mesh.vertices.push_back(v);
  }
  mesh.indices = {0, 1, 2, 0, 2, 3};
  scene.meshes.push_back(mesh);
  scene.draws.push_back({0, 0, 0, 6, glm::mat4(1), false, "flattened fixture"});
  RenderMaterial material;
  material.two_sided = true;
  Node sample, color, multiply;
  sample.op = Op::Sample;
  sample.texture = 0;
  color.op = Op::VertexColor;
  multiply.op = Op::Multiply;
  multiply.inputs = {0, 1};
  material.nodes = {sample, color, multiply};
  material.root = 2;
  scene.library.materials.push_back(material);
  TextureData texture;
  texture.width = texture.height = 2;
  texture.bytes = {255, 64, 32,  255, 32,  255, 64,  255,
                   64,  32, 255, 255, 192, 160, 128, 255};
  scene.library.textures.push_back(texture);
  return scene;
}
std::vector<std::uint8_t> raster(TerritoryRenderer &renderer,
                                 const VisualScene &scene, int tile,
                                 RasterInfo &info) {
  std::vector<std::uint8_t> result(128 * 128 * 3);
  info = renderer.render(
      scene, {128, tile, 4, true}, {}, {},
      [&](int y, int width, int, std::span<const std::uint8_t> bytes) {
        std::copy(bytes.begin(), bytes.end(), result.begin() + y * width * 3);
      });
  return result;
}
} // namespace

int gpu_flattened_draw_tests(territory::TerritoryRenderer &renderer) {
  using namespace territory;
  int failures = 0;
  for (int axis = 0; axis < 3; ++axis) {
    for (bool mirrored : {false, true}) {
      auto reference = plane_fixture(), collapsed = reference;
      auto &d = collapsed.draws[0];
      const int u = (axis + 1) % 3, v = (axis + 2) % 3;
      d.transform = glm::mat4(0);
      d.transform[u] = {mirrored ? -1.f : 1.f, 0, mirrored ? -.25f : .25f, 0};
      d.transform[v] = {0, 1, .5f, 0};
      d.transform[3] = {65536, 131072, 3, 1};
      const glm::vec2 xy[] = {{-20, -20}, {20, -20}, {20, 20}, {-20, 20}};
      for (int i = 0; i < 4; ++i) {
        auto &vertex = collapsed.meshes[0].vertices[i];
        vertex.position[axis] = 11.f * i - 18.f;
        vertex.position[u] = xy[i].x;
        vertex.position[v] = xy[i].y;
        // Not usable after collapse; geometry decides.
        vertex.normal = {1, 0, 0};
        if (mirrored) {
          auto &expected = reference.meshes[0].vertices[i];
          expected.position.x = 65536 - xy[i].x;
          expected.position.z = -.25f * xy[i].x + .5f * xy[i].y + 3;
          expected.normal = -expected.normal;
        }
      }
      // Cover both single-sided winding and transparent ordering/blending.
      reference.library.materials[0].two_sided = mirrored;
      collapsed.library.materials[0].two_sided = mirrored;
      if (axis == 1) {
        reference.library.materials[0].blend = Blend::Alpha;
        collapsed.library.materials[0].blend = Blend::Alpha;
      }
      bool valid = false;
      try {
        RasterInfo info;
        auto expected = raster(renderer, reference, 128, info);
        auto actual = raster(renderer, collapsed, 64, info);
        unsigned delta = 0;
        for (std::size_t i = 0; i < actual.size(); ++i)
          delta = std::max(
              delta, unsigned(std::abs(int(actual[i]) - int(expected[i]))));
        valid = delta <= 2 && actual[(64 * 128 + 64) * 3] != actual[0] &&
                info.issues.size() == 1 &&
                info.issues[0].kind == IssueKind::Simplified &&
                info.issues[0].source == d.source;
      } catch (const std::exception &e) {
        std::cout << "Flattened axis=" << axis << " mirror=" << mirrored << ": "
                  << e.what() << '\n';
      }
      failures += expect(valid, "flattened tilted mesh retains lit textured "
                                "pixels and reports geometric normals");
    }
  }
  // Nonzero but near-singular determinants use the same fallback. A negative
  // determinant must agree with the ordinary path's winding and normal sign.
  for (bool mirrored : {false, true}) {
    auto reference = plane_fixture(), tiny = reference;
    auto &d = tiny.draws[0];
    d.transform[0] = {mirrored ? -1.f : 1.f, 0, mirrored ? -.25f : .25f, 0};
    d.transform[1] = {0, 1, .5f, 0};
    d.transform[2] = {0, 0, 1e-14f, 0};
    d.transform[3] = {65536, 131072, 3, 1};
    for (int i = 0; i < 4; ++i) {
      auto &vertex = tiny.meshes[0].vertices[i];
      const float x = vertex.position.x - 65536;
      const float y = vertex.position.y - 131072;
      vertex.position = {x, y, 0};
      vertex.normal = {0, 0, 1};
      if (mirrored) {
        reference.meshes[0].vertices[i].position = {65536 - x, 131072 + y,
                                                    -.25f * x + .5f * y + 3};
      }
    }
    if (mirrored)
      reference.meshes[0].indices = {0, 2, 1, 0, 3, 2};
    reference.library.materials[0].two_sided = false;
    tiny.library.materials[0].two_sided = false;
    RasterInfo info;
    auto expected = raster(renderer, reference, 128, info);
    auto actual = raster(renderer, tiny, 64, info);
    unsigned delta = 0;
    for (std::size_t i = 0; i < actual.size(); ++i)
      delta = std::max(delta,
                       unsigned(std::abs(int(actual[i]) - int(expected[i]))));
    failures += expect(delta <= 2 && actual[(64 * 128 + 64) * 3] != actual[0] &&
                           info.issues.size() == 1 &&
                           info.issues[0].kind == IssueKind::Simplified,
                       "near-singular meshes retain winding and lit pixels, "
                       "including reflections");
  }
  {
    auto scene = plane_fixture();
    RasterInfo info;
    const auto expected = raster(renderer, scene, 128, info);
    auto line = scene.draws[0];
    line.source = "collapsed edge";
    line.transform = glm::mat4(0);
    line.transform[0] = {1.25f, -.5f, .75f, 0};
    line.transform[3] = {65536, 131072, 3, 1};
    scene.draws.push_back(line);
    const auto actual = raster(renderer, scene, 128, info);
    failures += expect(
        actual == expected && info.issues.size() == 1 &&
            info.issues[0].source == line.source &&
            info.issues[0].reason.find("retained 0 triangles") !=
                std::string::npos &&
            info.issues[0].reason.find("skipped 2 zero-area triangles") !=
                std::string::npos,
        "rank-one draw is skipped with counts and does not alter other draws");
  }
  return failures;
}

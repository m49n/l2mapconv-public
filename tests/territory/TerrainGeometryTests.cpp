#include "Fixtures.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <territory/TerrainGeometry.h>

int terrain_geometry_tests() {
  using namespace territory;
  int failures = 0;
  auto expect = [&](bool ok, const char *name) {
    if (!ok) {
      ++failures;
      std::cerr << "FAIL: " << name << '\n';
    }
  };
  TestDirectory directory;
  ArchiveFixture fixture(directory.path());
  auto make = [&](float x, float y, float z) {
    auto terrain = std::make_shared<unreal::TerrainInfoActor>(*fixture.archive);
    auto texture = std::make_shared<unreal::Texture>(*fixture.archive);
    texture->u_size = texture->v_size = 2;
    texture->format = unreal::TEXF_G16;
    texture->mips.push_back({0, {0, 0, 2, 0, 4, 0, 6, 0}, 2, 2, 1, 1});
    fixture.archive->export_map.push_back({fixture.names.name("Texture"),
                                           {},
                                           0,
                                           fixture.names.name("Height"),
                                           0,
                                           {},
                                           {},
                                           texture});
    unreal::Property ref{};
    ref.index_value.value =
        static_cast<int>(fixture.archive->export_map.size());
    terrain->terrain_map.from_property(ref, *fixture.archive);
    terrain->terrain_scale = {16384, 16384, 256};
    terrain->location = {x + 16384, y + 16384, z + 32768};
    terrain->quad_visibility_bitmap.insert({15});
    terrain->edge_turn_bitmap.insert({0});
    return terrain;
  };
  auto build =
      [](const std::array<std::shared_ptr<unreal::TerrainInfoActor>, 4> &tiles,
         const Bounds &bounds) {
        return build_terrain_mesh(tiles, bounds).mesh;
      };
  const Bounds crop{0, 0, 32768, 32768, -100, 100};
  std::array<std::shared_ptr<unreal::TerrainInfoActor>, 4> tiles{
      make(0, 0, 10), make(32768, 0, 100), make(0, 32768, 200),
      make(32768, 32768, 300)};
  const auto aligned = build(tiles, crop);
  expect(aligned.vertices.size() == 9 && aligned.indices.size() == 24,
         "regular terrain includes owned quads and neighbour borders");
  expect(aligned.vertices.at(4).position == glm::vec3(16384, 16384, 16),
         "owned heights use the owning terrain origin and scale");
  expect(aligned.vertices.at(2).position == glm::vec3(32768, 0, 100) &&
             aligned.vertices.at(6).position == glm::vec3(0, 32768, 200) &&
             aligned.vertices.at(8).position == glm::vec3(32768, 32768, 300),
         "border heights use each neighbour's Z transform");

  // Hand-derived translations include negative/non-client offsets. The crop
  // remains fixed, while geometry is never snapped to it.
  for (const auto offset : {glm::vec2(3, 3), glm::vec2(16, 16),
                            glm::vec2(-37, 11), glm::vec2(257, -129)}) {
    tiles[0]->location = {16384 + offset.x, 16384 + offset.y, 32778};
    try {
      const auto mesh = build(tiles, crop);
      expect(mesh.vertices.at(0).position == glm::vec3(offset, 10) &&
                 mesh.vertices.at(4).position ==
                     glm::vec3(16384 + offset.x, 16384 + offset.y, 16),
             "translated owned vertices preserve actual world coordinates");
      expect(glm::vec2(mesh.vertices.at(2).position) ==
                     glm::vec2(32768 + offset.x, offset.y) &&
                 glm::vec2(mesh.vertices.at(6).position) ==
                     glm::vec2(offset.x, 32768 + offset.y) &&
                 glm::vec2(mesh.vertices.at(8).position) ==
                     glm::vec2(32768 + offset.x, 32768 + offset.y),
             "world-sampled border preserves the regular owner grid");
      expect(mesh.vertices.at(2).uv[0] == glm::vec2(1, 0),
             "border alpha mask coordinates remain local to the owner grid");
      VisualScene scene;
      scene.bounds = crop;
      scene.terrain_mesh = 0;
      scene.meshes.push_back(mesh);
      expand_scene_z_bounds(scene);
      validate_scene(scene);
      expect(scene.bounds.min_x == 0 && scene.bounds.min_y == 0 &&
                 scene.bounds.max_x == 32768 && scene.bounds.max_y == 32768,
             "translated geometry does not move the radar crop");
    } catch (const std::exception &e) {
      std::cerr << "Translated terrain rejected: " << e.what() << '\n';
      expect(false, "valid translated terrain builds without map exceptions");
    }
  }

  tiles[0]->location = {16384, 16384, 32778};
  tiles[1]->location = {49157, 16377, 32868}; // neighbour origin (32773,-7,100)
  const auto shifted_neighbour = build(tiles, crop);
  expect(glm::vec2(shifted_neighbour.vertices.at(2).position) ==
                 glm::vec2(32768, 0) &&
             std::abs(shifted_neighbour.vertices.at(2).position.z -
                      100.001708984375f) < .00001f,
         "shifted neighbour sampled at owner XY with explicit uncovered-edge "
         "clamp");
  {
    // Production-scale counterexample: direct neighbour XYZ moves the border
    // behind the owner's last column when the displacement exceeds one step.
    std::array<std::shared_ptr<unreal::TerrainInfoActor>, 4> fine{
        make(0, 0, 10), make(32768, 0, 100), make(0, 32768, 200),
        make(32768, 32768, 300)};
    for (const auto &t : fine) {
      t->terrain_scale.x = t->terrain_scale.y = 128;
      auto tex = t->terrain_map.as<unreal::Texture>();
      tex->u_size = tex->v_size = 256;
      tex->mips[0].data.assign(256 * 256 * 2, 0);
      t->quad_visibility_bitmap.insert(std::vector<std::uint8_t>(8192, 255));
      t->edge_turn_bitmap.insert(std::vector<std::uint8_t>(8192, 0));
    }
    fine[1]->location.x -= 257;
    fine[3]->location.x -= 193;
    fine[3]->location.y -= 321;
    const auto mesh = build(fine, crop);
    bool forward = true;
    for (std::size_t i = 0; i < mesh.indices.size(); i += 3) {
      const auto a = mesh.vertices[mesh.indices[i]].position;
      const auto b = mesh.vertices[mesh.indices[i + 1]].position;
      const auto c = mesh.vertices[mesh.indices[i + 2]].position;
      forward = forward && glm::cross(b - a, c - a).z > 0;
    }
    expect(forward,
           "shifted neighbours never fold production-step border triangles");
  }
  {
    // At owner east border (32768,0), neighbour local point is (.5,.5).
    // Non-planar heights a=b=c=0,d=100 distinguish both diagonals.
    auto east = make(24576, -8192, 100);
    east->terrain_map->mips[0].data = {0, 0, 0, 0, 0, 0, 100, 0};
    const auto bc = build({tiles[0], east, {}, {}}, crop);
    expect(std::abs(bc.vertices.at(2).position.z - 100.f) < .0001f,
           "world-coordinate border sample follows BC heightfield triangle");
    east->edge_turn_bitmap.insert({1});
    const auto ad = build({tiles[0], east, {}, {}}, crop);
    expect(std::abs(ad.vertices.at(2).position.z - 150.f) < .0001f,
           "world-coordinate border sample follows AD heightfield triangle");
    east->terrain_map->mips[0].data = {0, 0, 20, 0, 40, 0, 100, 0};
    struct SampleCase {
      float x, y, ad, bc;
    };
    for (const auto point :
         {SampleCase{.75f, .25f, 135, 125}, SampleCase{.25f, .75f, 145, 135},
          SampleCase{.75f, .75f, 175, 165}}) {
      east->location.x = 49152 - point.x * 16384;
      east->location.y = 16384 - point.y * 16384;
      east->edge_turn_bitmap.insert({1});
      expect(
          std::abs(
              build({tiles[0], east, {}, {}}, crop).vertices.at(2).position.z -
              point.ad) < .0001f,
          "AD sampler covers both triangle halves");
      east->edge_turn_bitmap.insert({0});
      expect(
          std::abs(
              build({tiles[0], east, {}, {}}, crop).vertices.at(2).position.z -
              point.bc) < .0001f,
          "BC sampler covers both triangle halves");
    }
    expect(build_terrain_mesh({tiles[0], {}, {}, {}}, crop)
                   .clamped_border_samples == 5,
           "every unavailable border height is counted for output diagnostics");
  }

  const auto current = tiles[0];
  const auto without_neighbours = build({current, {}, {}, {}}, crop);
  expect(without_neighbours.vertices.at(2).position ==
                 glm::vec3(32768, 0, 12) &&
             without_neighbours.vertices.at(8).position ==
                 glm::vec3(32768, 32768, 16),
         "missing neighbours keep the documented last-owned-height fallback");
  current->quad_visibility_bitmap.insert({14});
  expect(build({current, {}, {}, {}}, crop).indices.size() == 18,
         "hidden terrain quad remains absent");
  current->quad_visibility_bitmap.insert({15});
  current->edge_turn_bitmap.insert({1});
  const auto turned = build({current, {}, {}, {}}, crop);
  expect(std::vector<std::uint32_t>(turned.indices.begin(),
                                    turned.indices.begin() + 6) ==
             std::vector<std::uint32_t>({0, 1, 4, 0, 4, 3}),
         "terrain diagonal bit retains the existing triangulation");
  current->location.x = std::numeric_limits<float>::quiet_NaN();
  expect(throws([&] { build(tiles, crop); }), "nonfinite terrain rejected");
  current->location.x = 81920;
  expect(throws([&] { build(tiles, crop); }),
         "nonoverlapping terrain rejected");
  current->location.x = 16384;
  current->terrain_scale.x = -16384;
  expect(throws([&] { build(tiles, crop); }), "negative XY scale rejected");
  current->terrain_scale.x = 8192;
  expect(throws([&] { build(tiles, crop); }),
         "unsupported terrain extent rejected");
  current->terrain_scale.x = 16384;
  current->terrain_map->mips[0].data.pop_back();
  expect(throws([&] { build(tiles, crop); }), "truncated G16 data rejected");
  return failures;
}

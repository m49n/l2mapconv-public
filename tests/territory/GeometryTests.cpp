#include "Fixtures.h"
#include <cmath>
#include <iostream>
#include <territory/TerrainMapping.h>
#include <territory/VisualSceneLoader.h>
#include <territory/WaterSurface.h>
int geometry_tests() {
  using namespace territory;
  int failures = 0;
  auto expect = [&](bool ok, const char *name) {
    if (!ok) {
      ++failures;
      std::cerr << "FAIL: " << name << '\n';
    }
  };
  expect(visual_bsp_visible(unreal::PF_NotSolid | unreal::PF_Translucent),
         "keep visible passable face");
  expect(visual_bsp_visible(0), "keep opaque face");
  expect(!visual_bsp_visible(unreal::PF_Invisible), "drop invisible face");
  expect(!visual_bsp_visible(unreal::PF_Portal | unreal::PF_NotSolid),
         "drop portal face");
  expect(!visual_bsp_visible(unreal::PF_FakeBackdrop), "drop sky portal");
  expect(!include_water(WaterEvidence::VolumeOnly, true),
         "volume is not a water surface");
  expect(include_water(WaterEvidence::Surface, true), "include actual water");
  expect(!include_water(WaterEvidence::Surface, false), "water switch");
  expect(!include_water(WaterEvidence::UnsupportedActor, true),
         "unsupported fluid does not create guessed mesh");
  const unreal::AssetReference water_material{
      "FX_E_T", "WaterSurfaceShaderSet.WaterShader01", "Shader"};
  const std::vector<WaterVolumeBounds> volumes = {
      {{0, 0, -100}, {100, 100, 0}, "test-volume"}};
  const std::vector<glm::vec3> polygon = {
      {10, 10, 0}, {40, 10, 0}, {40, 40, 0}};
  expect(water_surface(water_material, polygon, volumes) == "test-volume",
         "actual material and water level identify surface");
  expect(water_surface({"Other", "water_shader", "Shader"}, polygon, volumes)
             .empty(),
         "water substring alone never classifies a surface");
  expect(water_surface(water_material, polygon, {}).empty(),
         "material name alone does not establish water surface");
  auto elevated = polygon;
  for (auto &p : elevated)
    p.z = 20;
  expect(water_surface(water_material, elevated, volumes).empty(),
         "surface at different height is not volume water level");
  TestDirectory dir;
  ArchiveFixture fixture(dir.path());
  unreal::Actor actor(*fixture.archive);
  actor.location = {10, 20, 30};
  actor.pre_pivot = {2, 0, 0};
  actor.draw_scale = 2;
  actor.rotation.yaw = 16384;
  auto p = visual_actor_transform(actor) * glm::vec4(3, 0, 0, 1);
  expect(glm::length(glm::vec3(p) - glm::vec3(10, 22, 30)) < .001f,
         "pre-pivot is transformed with geometry before translation");
  auto refs = make_reference_fixture();
  unreal::StaticMesh mesh(*fixture.archive);
  mesh.materials.push_back({refs.a, false});
  actor.skins = {refs.b};
  expect(visual_skin(actor, mesh, 0).reference().object_path ==
             refs.b.reference().object_path,
         "instance skin overrides shared mesh");
  actor.skins.clear();
  expect(visual_skin(actor, mesh, 0).reference().object_path ==
             refs.a.reference().object_path,
         "collision-disabled visual material survives");
  VisualScene scene;
  {
    auto elevated = make_visual_fixture();
    elevated.draws.clear();
    elevated.terrain_mesh = 0;
    for (auto &vertex : elevated.meshes[0].vertices)
      vertex.position.z = 20000;
    expand_scene_z_bounds(elevated);
    expect(elevated.bounds.max_z > 20000,
           "terrain-only positive heights expand projection bounds");
    for (auto &vertex : elevated.meshes[0].vertices)
      vertex.position.z = -21000;
    expand_scene_z_bounds(elevated);
    expect(elevated.bounds.min_z < -21000,
           "terrain-only negative heights expand projection bounds");
  }
  scene.bounds = {0, 0, 32768, 32768, -100, 100};
  scene.draws.push_back({0, 0, 0, 3, glm::mat4(1.f), false, "test"});
  expect(throws([&] { validate_scene(scene); }), "out-of-range draw rejected");
  unreal::TerrainInfoActor terrain(*fixture.archive);
  unreal::Property layer{};
  layer.name = unreal::Name("Layers", 6);
  layer.array_index.value = 5;
  layer.subproperties.resize(1);
  terrain.set_property(layer);
  expect(terrain.layers.size() == 6, "sparse terrain slot keeps array index");
  expect(!terrain_mapping(terrain, unreal::TerrainLayer{}).verified,
         "unverified mapping not guessed");
  // Independent l2mapper UTerrainSector::Init: local grid / UScale *
  // (Layer.Scale/TerrainScale) * 2. P542 22_22 slots 0 and 1 serialize layer
  // Scale 128 and 32 respectively, terrain Scale 128.
  terrain.terrain_scale = {128, 128, 76};
  terrain.location = {81920, 147456, 160.65126f};
  auto tex = std::make_shared<unreal::Texture>(*fixture.archive);
  tex->u_size = 256;
  tex->v_size = 256;
  // Mapping tests exercise the supported fixed-size terrain transform without
  // requiring a parser reference to the heightmap.
  unreal::TerrainLayer mapping_layer{};
  mapping_layer.u_scale = 1;
  mapping_layer.v_scale = 1;
  mapping_layer.layer_scale = {32, 32, 76};
  // Map metadata identifies the square; the actual actor position defines UVs.
  terrain.map_x = 22;
  terrain.map_y = 22;
  auto mapping = terrain_mapping(terrain, mapping_layer);
  auto uv = mapping.world_to_uv * glm::vec4(65664, 131328, 0, 1);
  expect(mapping.verified && std::abs(uv.x - .5f) < .001f &&
             std::abs(uv.y - 1.f) < .001f,
         "P542 detail UV agrees with independent viewer control points");
  mapping_layer.layer_scale = {128, 128, 76};
  mapping = terrain_mapping(terrain, mapping_layer);
  uv = mapping.world_to_uv * glm::vec4(65664, 131328, 0, 1);
  expect(mapping.verified && std::abs(uv.x - 2.f) < .001f &&
             std::abs(uv.y - 4.f) < .001f,
         "P542 base layer uses its own serialized Scale");
  // Translation must move the color mapping with the geometry, independently
  // of the output square. These are not a table of production map exceptions.
  for (const auto offset : {glm::vec2(3, 3), glm::vec2(16, 16),
                            glm::vec2(-37, 11), glm::vec2(257, -129)}) {
    terrain.location = {81920 + offset.x, 147456 + offset.y, 160.65126f};
    mapping = terrain_mapping(terrain, mapping_layer);
    uv = mapping.world_to_uv *
         glm::vec4(65664 + offset.x, 131328 + offset.y, 0, 1);
    expect(mapping.verified && std::abs(uv.x - 2.f) < .001f &&
               std::abs(uv.y - 4.f) < .001f,
           "translated terrain keeps its own texture coordinates");
  }
  terrain.location = {81920, 147456, 160.65126f};
  mapping_layer.texture_map_axis = unreal::TEXMAPAXIS_XZ;
  expect(!terrain_mapping(terrain, mapping_layer).verified,
         "unevidenced terrain projection explicitly unsupported");
  mapping_layer.texture_map_axis = 0;
  mapping_layer.texture_rotation = 45;
  expect(!terrain_mapping(terrain, mapping_layer).verified,
         "unevidenced terrain rotation not guessed");
  return failures;
}

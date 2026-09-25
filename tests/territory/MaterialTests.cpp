#include "Fixtures.h"
#include "TestSupport.h"
#include <algorithm>
#include <territory/MaterialResolver.h>
#include <territory/VisualScene.h>
#include <unreal/Actor.h>
int material_tests() {
  using namespace territory;
  int failures = 0;
  {
    TestDirectory dir;
    ArchiveFixture f(dir.path(),
                     std::string("\x01\x05\x01\x02\x05\x02\x00", 7));
    f.archive->name_map.push_back(f.names.name("Diffuse"));
    f.archive->name_map.push_back(f.names.name("Opacity"));
    unreal::Shader native(*f.archive);
    native.flags = unreal::RF_Native;
    static_cast<std::istream &>(*f.archive).seekg(64);
    native.deserialize();
    failures += expect(
        native.diffuse.has_reference() && native.opacity.has_reference(),
        "RF_Native material still reads tagged diffuse and opacity properties");
  }
  failures +=
      expect(evaluate_node(Op::Multiply, {.2f, .4f, .6f, 1}, {.5f, .5f, .5f, 1},
                           0) == glm::vec4(.1f, .2f, .3f, 1),
             "combine both operands");
  failures += expect(evaluate_node(Op::Lerp, {0, 0, 0, 1}, {1, 1, 1, 1},
                                   .25f) == glm::vec4(.25f, .25f, .25f, 1),
                     "mask is linear data");
  failures += expect(evaluate_node(Op::AlphaReplace, {.1f, .2f, .3f, 1},
                                   {0, 0, 0, .25f},
                                   0) == glm::vec4(.1f, .2f, .3f, .25f),
                     "separate opacity replaces alpha only");
  RenderMaterial cycle;
  Node loop;
  loop.op = Op::Multiply;
  loop.inputs = {0, 0};
  cycle.nodes.push_back(loop);
  failures += expect(throws([&] { material_expression(cycle); }),
                     "graph cycle terminates");
  RenderMaterial invalid;
  invalid.root = 99;
  failures += expect(throws([&] { material_expression(invalid); }),
                     "reject invalid graph root");
  TestDirectory directory;
  ArchiveFixture fixture(directory.path());
  auto &archive = *fixture.archive;
  auto add = [&](std::shared_ptr<unreal::Object> object, const char *type,
                 const char *name) {
    object->name = fixture.names.name(name);
    object->flags = 0;
    archive.export_map.push_back(
        {fixture.names.name(type), {}, 0, object->name, 0, {}, {}, object});
    return static_cast<int>(archive.export_map.size());
  };
  auto property = [&](const char *name, int value) {
    unreal::Property p{};
    p.name = fixture.names.name(name);
    p.index_value.value = value;
    return p;
  };
  auto bitmap = std::make_shared<unreal::Texture>(archive);
  bitmap->format = unreal::TEXF_RGBA8;
  unreal::Mipmap mip{};
  mip.u_size = mip.v_size = 1;
  mip.data = {0, 0, 255, 64};
  bitmap->mips.push_back(mip);
  int bitmap_index = add(bitmap, "Texture", "Red");
  auto opacity = std::make_shared<unreal::Texture>(archive);
  opacity->format = unreal::TEXF_RGBA8;
  mip.data = {0, 0, 0, 128};
  opacity->mips.push_back(mip);
  int opacity_index = add(opacity, "Texture", "Opacity");
  auto scaler = std::make_shared<unreal::Modifier>(archive);
  scaler->set_property(property("Material", bitmap_index));
  auto scale_property = property("UScale", 0);
  scale_property.float_value = 2.f;
  failures += expect(scaler->set_property(scale_property),
                     "consume UV scaler property");
  int scaler_index = add(scaler, "TexScaler", "Scale");
  auto shader = std::make_shared<unreal::Shader>(archive);
  shader->set_property(property("Diffuse", scaler_index));
  failures += expect(shader->set_property(property("Opacity", opacity_index)),
                     "consume separate opacity");
  int shader_index = add(shader, "Shader", "Layered");
  MaterialLibrary library;
  Report report;
  MaterialResolver resolver(library, report);
  auto id = resolver.resolve(
      shader, archive.object_reference(unreal::Index{shader_index}), "actor:0");
  const auto &material = library.materials.at(id);
  failures +=
      expect(std::any_of(material.nodes.begin(), material.nodes.end(),
                         [](const Node &n) {
                           return n.op == Op::Sample && n.uv[0][0] == .5f;
                         }),
             "modifier below Diffuse survives with scaled UV");
  failures += expect(
      std::any_of(material.nodes.begin(), material.nodes.end(),
                  [](const Node &n) { return n.op == Op::AlphaReplace; }),
      "graph keeps opacity distinct");
  failures += expect(
      std::any_of(library.textures.begin(), library.textures.end(),
                  [](const auto &t) { return t.usage == TextureUsage::Mask; }),
      "opacity bitmap keeps data color-space");
  failures += expect(report.issues.empty(), "supported graph is not missing");
  failures += expect(material.shadow_coverage_reliable,
                     "supported material keeps reliable shadow coverage");
  {
    RenderMaterial uv_cutout;
    uv_cutout.blend = Blend::Masked;
    Node sample;
    sample.op = Op::Sample;
    sample.texture = 0;
    sample.uv_channel = 1;
    uv_cutout.nodes.push_back(sample);
    const auto missing = neutralize_missing_uv_samples(
        uv_cutout, std::array<bool, 4>{true, false, false, false});
    failures += expect(missing == std::vector<int>{1} &&
                           !uv_cutout.shadow_coverage_reliable &&
                           uv_cutout.nodes[0].op == Op::Constant,
                       "absent UV channel marks masked coverage unreliable");
    RenderMaterial supported_uv;
    supported_uv.nodes.push_back(sample);
    const auto available = neutralize_missing_uv_samples(
        supported_uv, std::array<bool, 4>{true, true, false, false});
    failures += expect(available.empty() &&
                           supported_uv.shadow_coverage_reliable &&
                           supported_uv.nodes[0].op == Op::Sample,
                       "available UV channel preserves reliable sample");
  }
  {
    VisualScene scene;
    scene.library = library;
    scene.library.materials.push_back(material);
    scene.draws = {{0, id, 0, 6, glm::mat4(1), false, "actor:0"},
                   {0, scene.library.materials.size() - 1, 0, 6, glm::mat4(1),
                    false, "actor:second"}};
    const auto inventory = material_inventory(scene);
    bool valid =
        inventory.contains("materials") && inventory.contains("textures");
    if (valid) {
      const auto &items = inventory["materials"];
      valid = items.size() == 1 && items[0]["source"] == "Fixture.Layered" &&
              items[0]["class"] == "Shader" &&
              items[0]["state"] == "supported" &&
              items[0]["surfaces"].size() == 2 &&
              items[0]["reference_chain"].size() == 4 &&
              items[0]["reference_chain"][2]["parent"] == "Fixture.Scale" &&
              items[0]["reference_chain"][2]["class"] == "Texture" &&
              inventory["textures"].size() == 2 &&
              inventory["textures"][0]["width"] == 1;
    }
    failures +=
        expect(valid, "production inventory preserves successful reference "
                      "chains, textures and deduplicated surface uses");
  }
  auto combiner = std::make_shared<unreal::Combiner>(archive);
  combiner->set_property(property("Material1", bitmap_index));
  combiner->set_property(property("Material2", opacity_index));
  auto multiply = property("CombineOperation", 2);
  failures += expect(combiner->set_property(multiply),
                     "consume combiner color operation");
  int combine_index = add(combiner, "Combiner", "Combined");
  auto ci = resolver.resolve(
      combiner, archive.object_reference(unreal::Index{combine_index}),
      "actor:1");
  failures +=
      expect(std::any_of(library.materials[ci].nodes.begin(),
                         library.materials[ci].nodes.end(),
                         [](const auto &n) { return n.op == Op::Multiply; }),
             "combiner graph multiplies both inputs");
  auto fb = std::make_shared<unreal::FinalBlend>(archive);
  fb->set_property(property("Material", bitmap_index));
  fb->fb_blending = unreal::FB_AlphaBlend;
  fb->z_write = false;
  fb->two_sided = true;
  int fb_index = add(fb, "FinalBlend", "Blend");
  auto fi = resolver.resolve(
      fb, archive.object_reference(unreal::Index{fb_index}), "actor:2");
  failures += expect(library.materials[fi].blend == Blend::Alpha &&
                         !library.materials[fi].depth_write &&
                         library.materials[fi].two_sided,
                     "FinalBlend retains depth and transparency flags");
  auto cutout_blend = std::make_shared<unreal::FinalBlend>(archive);
  cutout_blend->set_property(property("Material", bitmap_index));
  cutout_blend->fb_blending = unreal::FB_AlphaBlend;
  cutout_blend->alpha_test = true;
  int cutout_index = add(cutout_blend, "FinalBlend", "CutoutBlend");
  auto ai = resolver.resolve(
      cutout_blend, archive.object_reference(unreal::Index{cutout_index}),
      "cutout-alpha");
  failures += expect(library.materials[ai].blend == Blend::Alpha &&
                         library.materials[ai].alpha_test,
                     "alpha testing and alpha blending remain independent");
  auto cyclic = std::make_shared<unreal::Modifier>(archive);
  int cycle_index = add(cyclic, "TexScaler", "Cycle");
  cyclic->set_property(property("Material", cycle_index));
  resolver.resolve(cyclic, archive.object_reference(unreal::Index{cycle_index}),
                   "actor:cycle");
  failures += expect(std::any_of(report.issues.begin(), report.issues.end(),
                                 [](const Issue &i) {
                                   return i.kind == IssueKind::Unsupported;
                                 }),
                     "material reference cycle gets diagnostic");
  auto unknown = std::make_shared<unreal::Combiner>(archive);
  unknown->set_property(property("CombineOperation", 255));
  int unknown_index = add(unknown, "Combiner", "Unknown");
  auto before = report.issues.size();
  resolver.resolve(unknown,
                   archive.object_reference(unreal::Index{unknown_index}),
                   "actor:unknown");
  failures += expect(report.issues.size() > before,
                     "unknown enum is reported, not mapped to known op");
  auto masked_unknown = std::make_shared<unreal::Shader>(archive);
  masked_unknown->set_property(property("Diffuse", bitmap_index));
  masked_unknown->set_property(property("Opacity", unknown_index));
  masked_unknown->output_blending = unreal::OB_Masked;
  auto masked_unknown_index = add(masked_unknown, "Shader", "MaskedUnknown");
  auto masked_unknown_id = resolver.resolve(
      masked_unknown,
      archive.object_reference(unreal::Index{masked_unknown_index}),
      "actor:masked-unknown");
  failures += expect(!library.materials[masked_unknown_id].shadow_coverage_reliable,
                     "unsupported masked opacity cannot cast a solid shadow");
  auto masked_missing = std::make_shared<unreal::Shader>(archive);
  masked_missing->set_property(property("Diffuse", bitmap_index));
  masked_missing->set_property(property("Opacity", 999));
  masked_missing->output_blending = unreal::OB_Masked;
  auto masked_missing_index = add(masked_missing, "Shader", "MaskedMissing");
  auto masked_missing_id = resolver.resolve(
      masked_missing,
      archive.object_reference(unreal::Index{masked_missing_index}),
      "actor:masked-missing");
  failures += expect(!library.materials[masked_missing_id].shadow_coverage_reliable,
                     "missing masked opacity cannot cast a solid shadow");
  resolver.set_package_probe(
      [](std::string_view p) { return p != "AbsentPackage"; });
  resolver.resolve({}, {"AbsentPackage", "Tex", "Texture"}, "missing:package");
  resolver.resolve({}, {"PresentPackage", "Tex", "Texture"}, "missing:object");
  failures +=
      expect(std::any_of(report.issues.begin(), report.issues.end(),
                         [](const Issue &i) {
                           return i.kind == IssueKind::MissingPackage;
                         }) &&
                 std::any_of(report.issues.begin(), report.issues.end(),
                             [](const Issue &i) {
                               return i.kind == IssueKind::MissingObject;
                             }),
             "missing packages differ from missing objects");
  auto empty = std::make_shared<unreal::Texture>(archive);
  int empty_index = add(empty, "Texture", "EmptyBitmap");
  resolver.resolve(empty, archive.object_reference(unreal::Index{empty_index}),
                   "empty-bitmap");
  failures += expect(std::any_of(report.issues.begin(), report.issues.end(),
                                 [](const Issue &i) {
                                   return i.source == "Fixture.EmptyBitmap" &&
                                          i.kind == IssueKind::Unsupported;
                                 }),
                     "valid empty/procedural bitmap is unavailable data, not a "
                     "corrupt package");
  auto oscillator = std::make_shared<unreal::Modifier>(archive);
  oscillator->set_property(property("Material", bitmap_index));
  int oscillator_index = add(oscillator, "TexOscillator", "OscillatingLeaf");
  auto oi = resolver.resolve(
      oscillator, archive.object_reference(unreal::Index{oscillator_index}),
      "oscillating-leaf");
  failures +=
      expect(std::any_of(library.materials[oi].nodes.begin(),
                         library.materials[oi].nodes.end(),
                         [](const Node &n) { return n.op == Op::Sample; }) &&
                 std::any_of(report.issues.begin(), report.issues.end(),
                             [](const Issue &i) {
                               return i.source == "Fixture.OscillatingLeaf" &&
                                      i.kind == IssueKind::Simplified;
                             }),
             "unsupported animation keeps the bitmap with explicit "
             "unanimated-UV warning");
  auto triggered = std::make_shared<unreal::Modifier>(archive);
  triggered->set_property(property("Material", bitmap_index));
  auto triggered_index =
      add(triggered, "TexOscillatorTriggered", "TriggeredWater");
  auto ti = resolver.resolve(
      triggered, archive.object_reference(unreal::Index{triggered_index}),
      "triggered-water");
  failures +=
      expect(std::any_of(library.materials[ti].nodes.begin(),
                         library.materials[ti].nodes.end(),
                         [](const Node &n) { return n.op == Op::Sample; }),
             "triggered water oscillator keeps bitmap with explicit "
             "frozen-animation warning");
  auto incomplete = std::make_shared<unreal::Combiner>(archive);
  incomplete->set_property(property("Material1", bitmap_index));
  incomplete->set_property(property("Material2", unknown_index));
  incomplete->set_property(multiply);
  int incomplete_index = add(incomplete, "Combiner", "Partial");
  auto pi = resolver.resolve(
      incomplete, archive.object_reference(unreal::Index{incomplete_index}),
      "partial");
  failures += expect(
      std::any_of(library.materials[pi].nodes.begin(),
                  library.materials[pi].nodes.end(),
                  [](const Node &n) { return n.op == Op::Sample; }),
      "unknown graph branch does not discard a supported diffuse sibling");
  auto specular = std::make_shared<unreal::Shader>(archive);
  specular->set_property(property("Diffuse", bitmap_index));
  specular->set_property(property("Specular", opacity_index));
  int specular_index = add(specular, "Shader", "Specular");
  resolver.resolve(specular,
                   archive.object_reference(unreal::Index{specular_index}),
                   "specular:1");
  resolver.resolve(specular,
                   archive.object_reference(unreal::Index{specular_index}),
                   "specular:2");
  failures += expect(
      std::any_of(report.issues.begin(), report.issues.end(),
                  [](const Issue &i) {
                    return i.kind == IssueKind::Simplified &&
                           std::find(i.surfaces.begin(), i.surfaces.end(),
                                     "specular:1") != i.surfaces.end() &&
                           std::find(i.surfaces.begin(), i.surfaces.end(),
                                     "specular:2") != i.surfaces.end();
                  }),
      "cached simplified materials retain all referring actor slots");
  auto solid = std::make_shared<unreal::ConstantColor>(archive);
  auto color_property = property("Color", 0);
  color_property.color_value = {255, 0, 0, 128};
  failures += expect(solid->set_property(color_property),
                     "read non-textured ConstantColor");
  int solid_index = add(solid, "ConstantColor", "Solid");
  auto si = resolver.resolve(
      solid, archive.object_reference(unreal::Index{solid_index}), "solid");
  const auto &solid_material = library.materials[si];
  failures += expect(!solid_material.nodes.empty() &&
                         solid_material.nodes[solid_material.root].value ==
                             glm::vec4(1.f, 0.f, 0.f, 128.f / 255.f),
                     "normal solid color is not a missing texture");
  auto vertex = std::make_shared<unreal::Object>(archive);
  int vertex_index = add(vertex, "VertexColor", "Vertex");
  auto vi = resolver.resolve_object(
      vertex, archive.object_reference(unreal::Index{vertex_index}), "vertex");
  failures += expect(
      !library.materials[vi].nodes.empty() &&
          library.materials[vi].nodes[library.materials[vi].root].op ==
              Op::VertexColor,
      "VertexColor preserves mesh color input instead of grey placeholder");
  unreal::StaticMeshActor actor(archive);
  unreal::Property skins{};
  skins.name = fixture.names.name("Skins");
  skins.type = unreal::PropertyType::Array;
  skins.array_size.value = 2;
  skins.data_value = {static_cast<std::uint8_t>(bitmap_index),
                      static_cast<std::uint8_t>(opacity_index)};
  failures +=
      expect(actor.set_property(skins) && actor.skins.size() == 2 &&
                 actor.skins[1].as<unreal::Texture>() == opacity,
             "actor skin array retains per-instance material overrides");
  unreal::StaticMeshActor broken_actor(archive);
  skins.data_value.clear();
  failures += expect(throws([&] { broken_actor.set_property(skins); }),
                     "truncated skin array is not silently accepted");
  auto rotated = std::make_shared<unreal::Modifier>(archive);
  rotated->set_property(property("Material", bitmap_index));
  auto angle = property("Rotation", 0);
  angle.rotator_value = {0, 16384, 0};
  rotated->set_property(angle);
  int rotation_index = add(rotated, "TexRotator", "Rotated");
  auto ri = resolver.resolve(
      rotated, archive.object_reference(unreal::Index{rotation_index}),
      "rotated");
  auto sample = std::find_if(library.materials[ri].nodes.begin(),
                             library.materials[ri].nodes.end(),
                             [](const Node &n) { return n.op == Op::Sample; });
  failures += expect(sample != library.materials[ri].nodes.end() &&
                         glm::length(sample->uv * glm::vec3(1, 0, 1) -
                                     glm::vec3(0, 1, 1)) < .00001f,
                     "fixed quarter-turn UV matrix maps U axis to V");
  auto panned = std::make_shared<unreal::Modifier>(archive);
  panned->set_property(property("Material", bitmap_index));
  int panner_index = add(panned, "TexPanner", "Panned");
  auto pni = resolver.resolve(
      panned, archive.object_reference(unreal::Index{panner_index}), "panned");
  auto pan_sample = std::find_if(
      library.materials[pni].nodes.begin(), library.materials[pni].nodes.end(),
      [](const Node &n) { return n.op == Op::Sample; });
  failures += expect(pan_sample != library.materials[pni].nodes.end() &&
                         pan_sample->uv == glm::mat3(1.f),
                     "panning has no displacement at exported time zero");
  auto fallback = std::make_shared<unreal::Material>(archive);
  fallback->set_property(property("FallbackMaterial", bitmap_index));
  int fallback_index = add(fallback, "CustomMaterial", "Fallback");
  auto fbi = resolver.resolve(
      fallback, archive.object_reference(unreal::Index{fallback_index}),
      "fallback");
  failures +=
      expect(std::any_of(library.materials[fbi].nodes.begin(),
                         library.materials[fbi].nodes.end(),
                         [](const Node &n) { return n.op == Op::Sample; }) &&
                 std::any_of(report.issues.begin(), report.issues.end(),
                             [](const Issue &i) {
                               return i.source == "Fixture.Fallback" &&
                                      i.kind == IssueKind::Simplified;
                             }),
             "explicit fallback remains visible and labeled simplified");
  auto separate = std::make_shared<unreal::Shader>(archive);
  separate->set_property(property("Diffuse", bitmap_index));
  separate->set_property(property("Opacity", bitmap_index));
  int separate_index = add(separate, "Shader", "BothUsages");
  resolver.resolve(separate,
                   archive.object_reference(unreal::Index{separate_index}),
                   "both-usages");
  failures += expect(
      std::count_if(
          library.textures.begin(), library.textures.end(),
          [](const TextureData &t) { return t.source == "Fixture.Red"; }) == 2,
      "same bitmap color and mask usages do not alias GPU interpretation");
  return failures;
}

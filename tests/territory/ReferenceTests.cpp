#include "Fixtures.h"
#include "TestSupport.h"
#include <limits>
int reference_tests() {
  using namespace territory;
  int failures = 0;
  auto refs = make_reference_fixture();
  failures += expect(refs.a.reference().object_path == "GroupA.Same",
                     "retain outer path A");
  failures += expect(refs.b.reference().object_path == "GroupB.Same",
                     "retain outer path B");
  failures +=
      expect(refs.a.reference().package == "Shared", "retain imported package");
  failures += expect(refs.a.reference().class_name == "Shader",
                     "retain true material class");
  auto a = refs.a.untyped(), b = refs.b.untyped();
  failures +=
      expect(a && b && a != b, "equal leaves in different groups do not alias");
  failures += expect(a && a->asset_reference().object_path == "GroupA.Same",
                     "resolved object retains identity");
  failures += expect(a && a->serial_begin() == 512 && a->serial_end() == 513,
                     "export serial bounds are exact");
  failures +=
      expect(!refs.shader.as<unreal::Texture>(), "shader is not a bitmap");
  failures += expect(bool(refs.shader.as<unreal::Material>()),
                     "narrow lookup does not poison material");
  failures += expect(!refs.empty.has_reference() && !refs.empty.untyped(),
                     "null differs from missing reference");
  failures += expect(refs.missing.has_reference() && !refs.missing.untyped(),
                     "absent import retains identity");
  failures += expect(!refs.wrong_group.untyped(),
                     "absent group cannot use same leaf in another group");
  {
    TestDirectory directory;
    auto fixture = make_texture_fixture(directory.path());
    auto &target = *fixture->archive;
    target.export_map[0].object_name = fixture->names.name("g_01");
    target.export_map.push_back({fixture->names.name("Package"),
                                 {},
                                 0,
                                 fixture->names.name("Texture"),
                                 0,
                                 {},
                                 {},
                                 {}});
    target.export_map[0].package_index = 3;
    const auto exact = target.object_loader.load_object(
        {"Fixture", "Texture.g_01", "Texture"});
    failures += expect(bool(std::dynamic_pointer_cast<unreal::Texture>(exact)),
                       "lowercase texture export is a readable bitmap");
    failures += expect(target.object_loader.load_object(
                           {"Fixture", "Texture.G_01", "Texture"}) == exact,
                       "G_01 import resolves lowercase g_01 export");
    failures += expect(target.object_loader.load_object(
                           {"fIXTURE", "tEXTURE.g_01", "tEXTURE"}) == exact,
                       "reference package group and class ignore ASCII case");
    failures +=
        expect(exact && exact->asset_reference().object_path == "Texture.g_01",
               "case-insensitive lookup preserves serialized export spelling");
    failures += expect(
        !target.object_loader.load_object({"Fixture", "Other.G_01", "Texture"}),
        "case-insensitive lookup does not ignore the group");
    failures += expect(
        !target.object_loader.load_object({"Other", "Texture.G_01", "Texture"}),
        "case-insensitive lookup does not ignore the package");
    failures += expect(!target.object_loader.load_object(
                           {"Fixture", "Texture.G_01", "Shader"}),
                       "case-insensitive lookup does not ignore the class");
    failures +=
        expect(!target.object_loader.load_object(
                   {"Fixture", "Texture.G_01_extra", "Texture"}),
               "case-insensitive lookup requires the complete object name");
  }
  auto &archive = *refs.archive;
  archive.import_map[1].package_index = -2;
  failures +=
      expect(throws([&] { (void)archive.object_reference(unreal::Index{-4}); }),
             "reject cyclic outer chain");
  archive.import_map[1].package_index = 999;
  failures +=
      expect(throws([&] { (void)archive.object_reference(unreal::Index{-4}); }),
             "reject invalid outer chain");
  // Check extreme indices via reference metadata, before the unsafe old loader.
  failures += expect(throws([&] {
                       (void)archive.object_reference(
                           unreal::Index{std::numeric_limits<int>::min()});
                     }),
                     "reject minimum signed index without overflow");
  return failures;
}

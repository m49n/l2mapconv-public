#include "Fixtures.h"
#include <fstream>
#include <random>
#include <stdexcept>
namespace territory {
VisualScene make_visual_fixture() {
  VisualScene scene;
  scene.bounds = {0, 0, 32768, 32768, -128, 128};
  VisualMesh mesh;
  for (glm::vec3 p :
       {glm::vec3(0, 0, 0), glm::vec3(32768, 0, 0), glm::vec3(0, 32768, 0)}) {
    VisualVertex v;
    v.position = p;
    v.normal = {0, 0, 1};
    mesh.vertices.push_back(v);
  }
  mesh.indices = {0, 1, 2};
  scene.meshes.push_back(std::move(mesh));
  RenderMaterial material;
  material.nodes.emplace_back();
  scene.library.materials.push_back(material);
  scene.draws.push_back({0, 0, 0, 3, glm::mat4(1), false, "fixture"});
  return scene;
}
TestDirectory::TestDirectory()
    : parent(
          std::filesystem::canonical(std::filesystem::temp_directory_path())) {
  std::random_device random;
  for (int i = 0; i < 100; ++i) {
    auto candidate = parent / ("l2-territory-test-" + std::to_string(random()) +
                               "-" + std::to_string(random()));
    if (std::filesystem::create_directory(candidate)) {
      owned_path = std::filesystem::canonical(candidate);
      return;
    }
  }
  throw std::runtime_error("Unable to create owned test directory");
}
TestDirectory::~TestDirectory() {
  if (!owned_path.empty() && owned_path.parent_path() == parent &&
      owned_path.filename().string().starts_with("l2-territory-test-")) {
    std::error_code ec;
    std::filesystem::remove_all(owned_path, ec);
  }
}
namespace {
void integer(std::string &data, std::uint32_t n) {
  for (int shift = 0; shift < 32; shift += 8)
    data.push_back(static_cast<char>((n >> shift) & 255));
}
void index(std::string &data, int n) {
  unsigned value = n < 0 ? static_cast<unsigned>(-n) : static_cast<unsigned>(n);
  unsigned first = (value & 63) | (n < 0 ? 128 : 0);
  value >>= 6;
  data.push_back(static_cast<char>(first | (value ? 64 : 0)));
  while (value) {
    unsigned next = value & 127;
    value >>= 7;
    data.push_back(static_cast<char>(next | (value ? 128 : 0)));
  }
}
std::string header(int names, int name_offset, int exports, int export_offset,
                   int imports, int import_offset) {
  std::string bytes;
  integer(bytes, 0x9e2a83c1);
  integer(bytes, 100);
  integer(bytes, 0);
  for (int n :
       {names, name_offset, exports, export_offset, imports, import_offset})
    integer(bytes, n);
  bytes.resize(64, '\0');
  return bytes;
}
void shared_package(const std::filesystem::path &path) {
  std::string names;
  for (std::string n : {"None", "Core", "Class", "Shader", "Package", "GroupA",
                        "GroupB", "Same", "PlainShader"}) {
    index(names, static_cast<int>(n.size() + 1));
    names += n;
    names += '\0';
    integer(names, 0);
  }
  std::string imports;
  for (int n : {3, 4}) {
    index(imports, 1);
    index(imports, 2);
    integer(imports, 0);
    index(imports, n);
  }
  std::string exports;
  for (int i = 0; i < 5; ++i) {
    index(exports, i < 2 ? -2 : -1);
    index(exports, 0);
    integer(exports, i == 2 ? 1 : i == 3 ? 2 : 0);
    index(exports, i < 2 ? 5 + i : i < 4 ? 7 : 8);
    integer(exports, 0);
    index(exports, i < 2 ? 0 : 1);
    if (i >= 2)
      index(exports, 512 + i - 2);
  }
  auto bytes =
      header(9, 64, 5, static_cast<int>(64 + names.size() + imports.size()), 2,
             static_cast<int>(64 + names.size()));
  bytes += names;
  bytes += imports;
  bytes += exports;
  if (bytes.size() > 512)
    throw std::runtime_error("fixture table overlaps payload");
  bytes.resize(515, '\0');
  std::ofstream output(path, std::ios::binary);
  for (char c : std::string("Lineage2Ver111")) {
    output.put(c);
    output.put('\0');
  }
  for (unsigned char c : bytes)
    output.put(static_cast<char>(c ^ 0xac));
  if (!output)
    throw std::runtime_error("cannot write synthetic package");
}
} // namespace
ArchiveFixture::ArchiveFixture(const std::filesystem::path &root,
                               std::string payload)
    : loader(root, {unreal::SearchConfig{"", "utx"}}) {
  std::stringstream stream(header(0, 64, 0, 64, 0, 64) + payload);
  archive =
      std::make_unique<unreal::Archive>("Fixture", std::move(stream), loader);
  archive->name_map.push_back(names.name("None"));
}
MaterialRef material_reference(unreal::Archive &archive, int i) {
  unreal::Property property{};
  property.index_value.value = i;
  MaterialRef ref;
  ref.from_property(property, archive);
  return ref;
}
ReferenceFixture make_reference_fixture() {
  struct Storage {
    TestDirectory directory;
    ArchiveFixture source{directory.path()};
  };
  auto storage = std::make_shared<Storage>();
  shared_package(storage->directory.path() / "Shared.utx");
  auto &a = *storage->source.archive;
  auto &names = storage->source.names;
  auto imported = [&](const char *type, int outer, const char *name) {
    a.import_map.push_back(
        {names.name("Engine"), names.name(type), outer, names.name(name)});
  };
  imported("Package", 0, "Shared");
  imported("Package", -1, "GroupA");
  imported("Package", -1, "GroupB");
  imported("Shader", -2, "Same");
  imported("Shader", -3, "Same");
  imported("Shader", -1, "PlainShader");
  imported("Shader", -2, "Absent");
  imported("Package", -1, "MissingGroup");
  imported("Shader", -8, "Same");
  return {storage,
          material_reference(a, -4),
          material_reference(a, -5),
          material_reference(a, -6),
          {},
          material_reference(a, -7),
          material_reference(a, -9),
          &a};
}
std::unique_ptr<ArchiveFixture>
make_texture_fixture(const std::filesystem::path &root, int format,
                     int mip_count) {
  std::string data;
  index(data, 1);
  data.push_back(5);
  index(data, 2); // Palette reference to export 2
  if (format != 0) {
    index(data, 2);
    data.push_back(1);
    data.push_back(static_cast<char>(format));
  }
  for (int name : {3, 4}) {
    index(data, name);
    data.push_back(0x22);
    integer(data, 2);
  }
  index(data, 0); // end properties, omitted Format is P8
  index(data, mip_count);
  integer(data, 0); // standard lazy array header
  const auto data_size = format == 0 ? 4 : 8;
  index(data, data_size);
  for (int i = 0; i < data_size; ++i)
    data.push_back(i == 0 ? 7 : 0);
  integer(data, 2);
  integer(data, 2);
  data.push_back(1);
  data.push_back(1);
  const auto palette_offset = static_cast<int>(64 + data.size());
  index(data, 0);
  index(data, 256);
  for (int i = 0; i < 256; ++i) {
    for (int c : {10, 20, 30, 40})
      data.push_back(static_cast<char>(i == 7 ? c : 0));
  }
  auto result = std::make_unique<ArchiveFixture>(root, data);
  auto &a = *result->archive;
  for (auto name : {"Palette", "Format", "USize", "VSize"})
    a.name_map.push_back(result->names.name(name));
  a.export_map.push_back({result->names.name("Texture"),
                          {},
                          0,
                          result->names.name("Bitmap"),
                          0,
                          unreal::Index{palette_offset - 64},
                          unreal::Index{64},
                          {}});
  a.export_map.push_back(
      {result->names.name("Palette"),
       {},
       0,
       result->names.name("Colors"),
       0,
       unreal::Index{static_cast<int>(64 + data.size()) - palette_offset},
       unreal::Index{palette_offset},
       {}});
  return result;
}
} // namespace territory

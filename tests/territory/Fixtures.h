#pragma once
#include <exception>
#include <filesystem>
#include <territory/VisualScene.h>
#include <unreal/ArchiveLoader.h>
#include <unreal/Material.h>
namespace territory {
template <class F> bool throws(F &&f) {
  try {
    f();
  } catch (const std::exception &) {
    return true;
  }
  return false;
}
class TestDirectory {
public:
  TestDirectory();
  ~TestDirectory();
  const std::filesystem::path &path() const { return owned_path; }
  TestDirectory(const TestDirectory &) = delete;
  TestDirectory &operator=(const TestDirectory &) = delete;

private:
  std::filesystem::path owned_path, parent;
};
using MaterialRef =
    unreal::ObjectRef<unreal::Material, unreal::ObjectRefRequirement::Optional>;
struct ArchiveFixture {
  unreal::ArchiveLoader loader;
  unreal::NameTable names;
  std::unique_ptr<unreal::Archive> archive;
  explicit ArchiveFixture(const std::filesystem::path &root,
                          std::string payload = std::string(1, '\0'));
};
struct ReferenceFixture {
  std::shared_ptr<void> storage;
  MaterialRef a, b, shader, empty, missing, wrong_group;
  unreal::Archive *archive{};
};
ReferenceFixture make_reference_fixture();
MaterialRef material_reference(unreal::Archive &, int index);
std::unique_ptr<ArchiveFixture>
make_texture_fixture(const std::filesystem::path &, int format = 0,
                     int mip_count = 1);
VisualScene make_visual_fixture();
} // namespace territory

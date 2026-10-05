#include "NavmeshRegionGeometry.h"
#include "TestSupport.h"
#include <chrono>
#include <fstream>
#include <territory/PathIO.h>

int run_navmesh_region_geometry_tests() {
  int failures = 0;
  const auto root =
      std::filesystem::temp_directory_path() /
      ("nav-context-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root / "Maps");
  const std::vector<std::string> maps{"21_21", "21_22", "21_23",
                                      "22_21", "22_22", "22_23",
                                      "23_21", "23_22", "23_23"};
  for (const auto &name : maps)
    std::ofstream(root / "Maps" / (name + ".unr")) << name;
  std::ofstream(root / "shared.usx") << "shared";
  std::vector<std::string> seen;
  bool mutate = false, unreadable = false;
  CollisionMapLoad loader = [&](const std::string &name,
                                unreal::ArchiveReadObserver observe) {
    seen.push_back(name);
    if (unreadable && name == "22_21")
      throw std::runtime_error("unreadable neighbor");
    const auto file = root / "Maps" / (name + ".unr");
    observe(file, unreal::ArchiveReadPhase::Before);
    observe(file, unreal::ArchiveReadPhase::After);
    observe(root / "shared.usx", unreal::ArchiveReadPhase::Before);
    if (mutate)
      std::ofstream(root / "shared.usx", std::ios::app) << "changed";
    observe(root / "shared.usx", unreal::ArchiveReadPhase::After);
    const float x = (std::stoi(name.substr(0, 2)) - 20) * 32768.f,
                y = (std::stoi(name.substr(3, 2)) - 18) * 32768.f;
    geodata::Map map{name,
                     geometry::Box{{x, y, -16}, {x + 32768, y + 32768, 512}}};
    auto mesh = std::make_shared<geodata::Mesh>();
    mesh->vertices = {{{x, y, 0}, {0, 0, 1}},
                      {{x + 32768, y, 0}, {0, 0, 1}},
                      {{x, y + 32768, 0}, {0, 0, 1}}};
    mesh->indices = {0, 1, 2};
    mesh->instance_matrices.push_back(glm::mat4{1});
    map.add({mesh, glm::mat4{1}});
    return map;
  };
  auto rejects = [&](auto work, const char *message) {
    bool rejected = false;
    try {
      work();
    } catch (const std::exception &) {
      rejected = true;
    }
    failures += expect(rejected, message);
  };
  try {
    auto region = load_navmesh_region_geometry(root, "22_22", {}, {}, loader);
    failures += expect(seen.size() == 9 && seen.front() == "22_22" &&
                           region.metadata.loaded_neighbors.size() == 8 &&
                           region.metadata.sources.size() == 10,
                       "all eight neighbors loaded sequentially with actual "
                       "package identities");
    const auto file = root / "22_22.navmesh";
    // Metadata binds actual bytes; fixture mesh uses a small portion of own
    // output.
    region.geometry.game_bounds =
        geometry::Box{{65536, 131072, -16}, {66560, 132096, 512}};
    auto built = navmesh::build(region.geometry, {});
    navmesh::save(*built.mesh, file);
    region.metadata.generator = "test";
    region.metadata.mesh = territory::file_identity(file);
    navmesh::write_region_metadata_new(region.metadata, file);
    auto read = navmesh::read_region_metadata(file);
    failures += expect(read.map == "22_22" && read.padding == 64 &&
                           read.sources.size() == 10 &&
                           read.mesh.sha256 == region.metadata.mesh.sha256,
                       "sidecar profile source and hash roundtrip");
    rejects([&] { navmesh::write_region_metadata_new(region.metadata, file); },
            "sidecar cannot overwrite");
    const auto cancelled = root / "cancel.navmesh";
    navmesh::save(*built.mesh, cancelled);
    region.metadata.mesh = territory::file_identity(cancelled);
    rejects(
        [&] {
          navmesh::write_region_metadata_new(region.metadata, cancelled,
                                             [] { return true; });
        },
        "cancel before marker publication");
    failures +=
        expect(!std::filesystem::exists(navmesh::metadata_path(cancelled)),
               "cancel leaves no completion marker");
    std::ofstream(file, std::ios::app) << 'x';
    rejects([&] { navmesh::read_region_metadata(file); },
            "changed mesh hash rejected");
  } catch (const std::exception &e) {
    failures += expect(false, e.what());
  }
  std::filesystem::remove(root / "Maps" / "23_23.unr");
  try {
    auto partial = load_navmesh_region_geometry(root, "22_22", {}, {}, loader);
    failures += expect(partial.metadata.missing_neighbors ==
                           std::vector<std::string>{"23_23"},
                       "missing outer context is recorded");
  } catch (const std::exception &e) {
    failures += expect(false, e.what());
  }
  unreadable = true;
  rejects([&] { load_navmesh_region_geometry(root, "22_22", {}, {}, loader); },
          "present unreadable neighbor fails");
  unreadable = false;
  mutate = true;
  rejects([&] { load_navmesh_region_geometry(root, "22_22", {}, {}, loader); },
          "package mutation during read fails");
  mutate = false;
  rejects(
      [&] {
        load_navmesh_region_geometry(
            root, "22_22", {}, [] { return true; }, loader);
      },
      "cancel stops neighbor loading");
  auto fractional = navmesh::Settings{};
  fractional.cell_size = 16.1f;
  rejects(
      [&] {
        load_navmesh_region_geometry(root, "22_22", fractional, {}, loader);
      },
      "partial tile grid rejected");
  std::filesystem::remove_all(root);
  return failures;
}

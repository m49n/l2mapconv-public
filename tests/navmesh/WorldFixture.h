#pragma once
#include <array>
#include <chrono>
#include <navmesh/WorldMesh.h>
namespace world_fixture {
struct Directory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("nav-world-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  Directory() { std::filesystem::create_directories(path); }
  ~Directory() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};
struct Geometry {
  std::shared_ptr<geodata::Mesh> mesh = std::make_shared<geodata::Mesh>();
  Geometry() { mesh->instance_matrices.push_back(glm::mat4{1}); }
  void quad(std::array<glm::vec3, 4> points, glm::vec3 normal) {
    const auto base = unsigned(mesh->vertices.size());
    for (auto p : points)
      mesh->vertices.push_back({p, normal});
    for (auto i : {0u, 1u, 2u, 0u, 2u, 3u})
      mesh->indices.push_back(base + i);
  }
  void floor(float x0, float y0, float x1, float y1, float z = 0) {
    quad({{{x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}}}, {0, 0, 1});
  }
  void wall(float x, float y0, float y1) {
    quad({{{x, y0, -16}, {x, y1, -16}, {x, y1, 192}, {x, y0, 192}}}, {1, 0, 0});
  }
  geodata::Map map(const std::string &name) {
    const auto b = navmesh::region_bounds(name);
    geodata::Map out{name, geometry::Box{{b.x, b.y, -16}, {b.z, b.w, 512}}};
    out.add({mesh, glm::mat4{1}});
    return out;
  }
};
inline navmesh::RegionFile save(Geometry &g,
                                const std::filesystem::path &directory,
                                const std::string &name,
                                navmesh::Settings settings = {}) {
  auto map = g.map(name);
  auto built = navmesh::build(map, settings);
  const auto file = directory / (name + ".navmesh");
  navmesh::save(*built.mesh, file);
  navmesh::RegionMetadata m;
  m.map = name;
  m.generator = "world-test";
  m.settings = settings;
  m.padding = navmesh::context_padding(settings);
  m.neighbor_context = true;
  m.loaded_neighbors = navmesh::region_neighbors(name);
  auto sources = m.loaded_neighbors;
  sources.push_back(name);
  for (const auto &n : sources)
    m.sources.push_back({"maps/" + n + ".unr", std::string(64, 'a'), 1});
  m.mesh = territory::file_identity(file);
  navmesh::write_region_metadata_new(m, file);
  return {name, file};
}
} // namespace world_fixture

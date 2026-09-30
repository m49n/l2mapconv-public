#pragma once
#include "Diagnostics.h"
#include "MaterialGraph.h"
#include <array>
#include <glm/glm.hpp>
#include <optional>
#include <vector>
namespace territory {
struct Bounds {
  double min_x{}, min_y{}, max_x{}, max_y{}, min_z{}, max_z{};
};
struct VisualVertex {
  glm::vec3 position{}, normal{};
  std::array<glm::vec2, 4> uv{};
  glm::vec4 color{1.f};
};
struct VisualMesh {
  std::vector<VisualVertex> vertices;
  std::vector<std::uint32_t> indices;
};
struct Draw {
  std::size_t mesh{}, material{}, first_index{}, index_count{};
  glm::mat4 transform{1.f};
  bool water{};
  std::string source;
  // Collision classification for the untextured geometry preview only.
  bool passable{};
};
struct TerrainLayer {
  std::size_t material{}, mask_texture{};
  glm::mat4 world_to_uv{1.f};
};
struct VisualScene {
  Bounds bounds;
  std::vector<VisualMesh> meshes;
  std::vector<Draw> draws;
  std::vector<TerrainLayer> terrain_layers;
  std::optional<std::size_t> terrain_mesh;
  MaterialLibrary library;
  Report report;
};
void validate_scene(const VisualScene &);
Json material_inventory(const VisualScene &);
void expand_scene_z_bounds(VisualScene &);
} // namespace territory

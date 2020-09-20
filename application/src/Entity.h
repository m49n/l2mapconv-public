#pragma once

#include <math/Box.h>
#include <math/Transformation.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

enum SurfaceType {
  SURFACE_PASSABLE = 1,
  SURFACE_TERRAIN = 2,
  SURFACE_STATIC_MESH = 4,
  SURFACE_CSG = 8,
  SURFACE_BOUNDING_BOX = 16,
};

struct Texture {
  int width;
  int height;
  std::vector<std::uint8_t> texels;
};

struct Material {
  glm::vec3 color;
  std::shared_ptr<Texture> texture;
};

struct Surface {
  std::uint64_t type;
  std::size_t index_offset;
  std::size_t index_count;
  Material material;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<Surface> surfaces;
  math::Box bounding_box;
};

struct Entity {
  std::shared_ptr<Mesh> mesh;
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;
  bool wireframe;

  explicit Entity(std::shared_ptr<Mesh> mesh)
      : mesh{mesh}, position{}, rotation{}, scale{1.0f}, wireframe{false} {}

  auto model_matrix() const -> glm::mat4 {
    return math::transformation_matrix(position, rotation, scale);
  }
};

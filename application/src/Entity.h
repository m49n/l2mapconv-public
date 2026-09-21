#pragma once

#include <geodata/Geodata.h>

#include <geometry/Box.h>
#include <geometry/Transformation.h>

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

enum SurfaceType {
  SURFACE_PASSABLE = 0x1,
  SURFACE_TERRAIN = 0x2,
  SURFACE_STATIC_MESH = 0x4,
  SURFACE_CSG = 0x8,
  SURFACE_BOUNDING_BOX = 0x10,
  SURFACE_IMPORTED_GEODATA = 0x20,
  SURFACE_GENERATED_GEODATA = 0x40,
  SURFACE_BLOCKING_VOLUME = 0x80,
};

enum TextureFormat {
  TEXTURE_RGBA,
  TEXTURE_DXT1,
  TEXTURE_DXT3,
  TEXTURE_DXT5,
};

struct Texture {
  TextureFormat format;
  int width;
  int height;
  const unsigned char *data;
};

struct Material {
  glm::vec3 color;
  Texture texture;
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

struct EntityMesh {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  std::vector<Surface> surfaces;
  std::vector<glm::mat4> instance_matrices;
  geometry::Box bounding_box;
};

struct GeodataMesh {
  std::vector<geodata::Cell> cells;
  Surface surface;
  geometry::Box bounding_box;
};

template <typename T> struct Entity {
  std::shared_ptr<T> mesh;
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;
  bool wireframe;

  explicit Entity(std::shared_ptr<T> mesh)
      : mesh{mesh}, position{}, rotation{}, scale{1.0f}, wireframe{false} {}

  auto model_matrix() const -> glm::mat4 {
    return geometry::transformation_matrix(glm::mat4{1.0f}, position, rotation,
                                           scale);
  }

  auto instance_matrices() const -> std::vector<glm::mat4> {
    return mesh->instance_matrices.empty() ? std::vector{glm::mat4{1.0f}}
                                           : mesh->instance_matrices;
  }
};

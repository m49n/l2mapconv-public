#pragma once

#include <geodata/Geodata.h>

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
  SURFACE_GEODATA = 32,
};

enum TextureFormat {
  TEXTURE_RGBA,
  TEXTURE_DXT1,
  TEXTURE_DXT3,
  TEXTURE_DXT5,
};

struct Texture {
  TextureFormat format;
  std::size_t width;
  std::size_t height;
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
  std::vector<std::uint32_t> indices;
  std::vector<Surface> surfaces;
  std::vector<glm::mat4> model_matrices;
  math::Box bounding_box;
};

struct GeodataMesh {
  std::vector<geodata::Block> blocks;
  Surface surface;
  math::Box bounding_box;
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
    //    // Swap Y-up with Z-up.
    //    auto identity = glm::scale(
    //        glm::mat4{
    //            {1.0f, 0.0f, 0.0f, 0.0f},
    //            {0.0f, 0.0f, 1.0f, 0.0f},
    //            {0.0f, 1.0f, 0.0f, 0.0f},
    //            {0.0f, 0.0f, 0.0f, 1.0f},
    //        },
    //        {1.0f, -1.0f, 1.0f});

    return math::transformation_matrix(glm::mat4{1.0f}, position, rotation,
                                       scale);
  }
};

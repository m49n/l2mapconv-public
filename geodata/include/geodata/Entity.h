#pragma once

#include <glm/glm.hpp>

#include <math/Box.h>

#include <cstdint>
#include <vector>

namespace geodata {

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
};

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<glm::mat4> model_matrices;
  math::Box bounding_box;
};

struct Entity {
  Mesh mesh;
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;
};

} // namespace geodata

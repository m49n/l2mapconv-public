#pragma once

#include "Texture.h"

#include <glm/glm.hpp>

#include <memory>
#include <utility>

namespace rendering {

struct Material {
  glm::vec3 color;
  std::shared_ptr<Texture> texture;
};

} // namespace rendering

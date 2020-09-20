#pragma once

#include "Texture.h"

#include <glm/glm.hpp>

#include <memory>
#include <utility>

namespace rendering {

struct Material {
  glm::vec3 color;
  std::shared_ptr<Texture> texture;

  Material() : color{}, texture{nullptr} {}
  explicit Material(const glm::vec3 &color) : color{color}, texture{nullptr} {}
  explicit Material(std::shared_ptr<Texture> texture)
      : color{}, texture{std::move(texture)} {}
};

} // namespace rendering

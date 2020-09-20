#pragma once

#include "Box.h"

#include <glm/glm.hpp>

#include <array>

namespace math {

class Frustum {
public:
  // matrix = projection_matrix * view_matrix
  explicit Frustum(const glm::mat4 &matrix);

  auto intersects(const Box &box) const -> bool;

private:
  std::array<glm::vec4, 6> m_planes;
};

} // namespace math

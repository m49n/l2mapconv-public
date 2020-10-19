#pragma once

#include <glm/glm.hpp>

namespace math {

auto transformation_matrix(const glm::mat4 &identity, const glm::vec3 &position,
                           const glm::vec3 &rotation, const glm::vec3 &scale)
    -> glm::mat4;

} // namespace math

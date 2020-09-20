#include <math/Transformation.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace math {

auto transformation_matrix(const glm::vec3 &position, const glm::vec3 &rotation,
                           const glm::vec3 &scale) -> glm::mat4 {

  // Swap Y-up with Z-up.
  auto matrix = glm::scale(
      glm::mat4{
          {1.0f, 0.0f, 0.0f, 0.0f},
          {0.0f, 0.0f, 1.0f, 0.0f},
          {0.0f, 1.0f, 0.0f, 0.0f},
          {0.0f, 0.0f, 0.0f, 1.0f},
      },
      {1.0f, -1.0f, 1.0f});

  matrix = glm::translate(matrix, position);
  matrix *= glm::mat4_cast(glm::quat{rotation});
  matrix = glm::scale(matrix, scale);

  return matrix;
}

} // namespace math

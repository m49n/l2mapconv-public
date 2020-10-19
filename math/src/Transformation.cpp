#include <math/Transformation.h>

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace math {

auto transformation_matrix(const glm::mat4 &identity, const glm::vec3 &position,
                           const glm::vec3 &rotation, const glm::vec3 &scale)
    -> glm::mat4 {

  auto matrix = glm::translate(identity, position);
  matrix *= glm::mat4_cast(glm::quat{rotation});
  matrix = glm::scale(matrix, scale);

  return matrix;
}

} // namespace math

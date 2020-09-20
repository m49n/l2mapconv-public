#include <math/Frustum.h>

namespace math {

Frustum::Frustum(const glm::mat4 &matrix) : m_planes{} {
  const auto transposed = glm::transpose(matrix);

  m_planes[0] = transposed[3] + transposed[0];
  m_planes[1] = transposed[3] - transposed[0];
  m_planes[2] = transposed[3] + transposed[1];
  m_planes[3] = transposed[3] - transposed[1];
  m_planes[4] = transposed[3] + transposed[2];
  m_planes[5] = transposed[3] - transposed[2];
}

auto Frustum::intersects(const Box &box) const -> bool {
  const auto &min = box.min();
  const auto &max = box.max();

  for (const auto &plane : m_planes) {
    const auto distance = std::max(min.x * plane.x, max.x * plane.x) +
                          std::max(min.y * plane.y, max.y * plane.y) +
                          std::max(min.z * plane.z, max.z * plane.z) + plane.w;

    if (distance <= 0) {
      return false;
    }
  }

  return true;
}

} // namespace math

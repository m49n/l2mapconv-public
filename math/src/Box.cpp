#include <math/Box.h>

#include <utils/Assert.h>

#include <algorithm>

namespace math {

Box::Box() : m_min{}, m_max{}, m_valid{false} {}

Box::Box(const glm::vec3 &min, const glm::vec3 &max)
    : m_min{min}, m_max{max}, m_valid{true} {}

Box::Box(const std::vector<glm::vec3> &vertices, const glm::mat4 &model_matrix)
    : Box{} {

  ASSERT(!vertices.empty(), "Math", "Vertices must not be empty");

  for (const auto &vertex : vertices) {
    *this += model_matrix * glm::vec4{vertex, 1.0f};
  }
}

Box::Box(const std::vector<glm::vec3> &vertices)
    : Box{vertices, glm::mat4{1.0f}} {}

Box::Box(const glm::vec3 &min, const glm::vec3 &max,
         const glm::mat4 &model_matrix)
    : Box{{
              {min.x, min.y, min.z},
              {min.x, max.y, min.z},
              {max.x, min.y, min.z},
              {max.x, max.y, min.z},
              {min.x, min.y, max.z},
              {min.x, max.y, max.z},
              {max.x, min.y, max.z},
              {max.x, max.y, max.z},
          },
          model_matrix} {}

Box::Box(const Box &box, const glm::mat4 &model_matrix)
    : Box{box.min(), box.max(), model_matrix} {}

auto Box::min() const -> const glm::vec3 & { return m_min; }
auto Box::max() const -> const glm::vec3 & { return m_max; }

auto Box::valid() const -> bool { return m_valid; }

auto Box::operator+=(const glm::vec3 &point) -> Box & {
  if (m_valid) {
    m_min.x = std::min(m_min.x, point.x);
    m_min.y = std::min(m_min.y, point.y);
    m_min.z = std::min(m_min.z, point.z);

    m_max.x = std::max(m_max.x, point.x);
    m_max.y = std::max(m_max.y, point.y);
    m_max.z = std::max(m_max.z, point.z);
  } else {
    m_min = m_max = point;
    m_valid = true;
  }

  return *this;
}

auto Box::contains(const glm::vec3 &point) const -> bool {
  return point.x > m_min.x && point.y > m_min.y && point.z > m_min.z &&
         point.x < m_max.x && point.y < m_max.y && point.z < m_max.z;
}

} // namespace math

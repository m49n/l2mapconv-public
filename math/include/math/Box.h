#pragma once

#include <glm/glm.hpp>

#include <vector>

namespace math {

class Box {
public:
  explicit Box();

  explicit Box(const glm::vec3 &min, const glm::vec3 &max);

  explicit Box(const std::vector<glm::vec3> &vertices,
               const glm::mat4 &model_matrix);

  explicit Box(const glm::vec3 &min, const glm::vec3 &max,
               const glm::mat4 &model_matrix);

  explicit Box(const Box &box, const glm::mat4 &model_matrix);

  explicit Box(const std::vector<glm::vec3> &vertices);

  auto min() const -> const glm::vec3 &;
  auto max() const -> const glm::vec3 &;

  auto is_zero() const -> bool;
  auto contains(const glm::vec3 &point) const -> bool;

  auto operator+=(const glm::vec3 &point) -> Box &;

private:
  glm::vec3 m_min;
  glm::vec3 m_max;
  bool m_valid;
};

} // namespace math

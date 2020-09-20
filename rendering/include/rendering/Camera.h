#pragma once

#include "Context.h"

#include <utils/NonCopyable.h>

#include <math/Frustum.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace rendering {

class Camera : public utils::NonCopyable {
public:
  explicit Camera(Context &context, float fov, float near,
                  const glm::vec3 &position)
      : m_context{context}, m_fov{fov}, m_near{near}, m_position{position},
        m_orientation{glm::quatLookAt(glm::vec3{0.0f, 0.0f, -1.0f},
                                      glm::vec3{0.0f, 1.0f, 0.0f})} {}

  void translate(const glm::vec3 &direction);
  void rotate(float angle, const glm::vec3 &axis);

  auto position() const -> const glm::vec3 &;

  auto forward() const -> glm::vec3;
  auto right() const -> glm::vec3;
  auto up() const -> glm::vec3;

  auto view_matrix() const -> glm::mat4;
  auto projection_matrix() const -> glm::mat4;

  auto frustum() const -> math::Frustum;

private:
  Context &m_context;

  float m_fov;
  float m_near;

  glm::vec3 m_position;
  glm::quat m_orientation;
};

} // namespace rendering

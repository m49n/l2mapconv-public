#pragma once

#include "Context.h"

#include <geometry/Frustum.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace rendering {

struct CameraState {
  glm::vec3 position;
  glm::quat orientation;
  bool top{};
  float width{4096}, z_min{-32768}, z_max{32768};
};

class Camera {
public:
  explicit Camera(Context &context, float fov, float near,
                  const glm::vec3 &position);

  void translate(const glm::vec3 &direction);
  void set_position(const glm::vec3 &position);
  void rotate(float angle, const glm::vec3 &axis);

  auto position() const -> const glm::vec3 &;

  auto forward() const -> glm::vec3;
  auto right() const -> glm::vec3;
  auto up() const -> glm::vec3;

  auto view_matrix() const -> glm::mat4;
  auto projection_matrix() const -> glm::mat4;

  auto frustum() const -> geometry::Frustum;
  auto snapshot() const -> CameraState;
  void restore(const CameraState &);
  void set_top_view(glm::vec2 center, float width, float z_min, float z_max);
  bool top_view() const { return m_top; }

private:
  const glm::vec3 m_up = {0.0f, 0.0f, 1.0f};

  Context &m_context;

  const float m_fov;
  const float m_near;

  glm::vec3 m_position;
  glm::quat m_orientation;
  bool m_top{};
  float m_width{4096}, m_z_min{-32768}, m_z_max{32768};
};

} // namespace rendering

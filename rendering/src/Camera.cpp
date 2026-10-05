#include "pch.h"

#include <rendering/Camera.h>

namespace rendering {

auto Camera::snapshot() const -> CameraState { return {m_position,m_orientation,m_top,m_width,m_z_min,m_z_max}; }
void Camera::restore(const CameraState &s) {
  m_position=s.position;m_orientation=s.orientation;m_top=s.top;
  m_width=s.width;m_z_min=s.z_min;m_z_max=s.z_max;
}
void Camera::set_top_view(glm::vec2 center, float width, float z_min, float z_max) {
  if(!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(width) ||
     !std::isfinite(z_min) || !std::isfinite(z_max) || width<=0 || z_min>=z_max)
    throw std::invalid_argument("Invalid top view bounds");
  m_top=true;m_position={center,z_max+1};m_width=width;m_z_min=z_min;m_z_max=z_max;
}

Camera::Camera(Context &context, float fov, float near,
               const glm::vec3 &position)
    : m_context{context}, m_fov{fov}, m_near{near}, m_position{position},
      m_orientation{glm::quatLookAt({0.0f, 1.0f, 0.0f}, m_up)} {}

void Camera::translate(const glm::vec3 &direction) { m_position += direction; }

void Camera::set_position(const glm::vec3 &position) { m_position = position; }

void Camera::rotate(float angle, const glm::vec3 &axis) {
  m_orientation = glm::normalize(glm::angleAxis(angle, axis) * m_orientation);
}

auto Camera::position() const -> const glm::vec3 & { return m_position; }

auto Camera::forward() const -> glm::vec3 {
  return m_orientation * glm::vec3{0.0f, 0.0f, -1.0f};
}

auto Camera::right() const -> glm::vec3 {
  return m_orientation * glm::vec3{1.0f, 0.0f, 0.0f};
}

auto Camera::up() const -> glm::vec3 { return m_up; }

auto Camera::view_matrix() const -> glm::mat4 {
  if(m_top) return glm::translate(glm::mat4{1},glm::vec3{-m_position.x,-m_position.y,0});
  return glm::translate(glm::mat4_cast(glm::conjugate(m_orientation)),
                        -m_position);
}

auto Camera::projection_matrix() const -> glm::mat4 {
  if(m_top) {
    const float aspect=float(std::max(1,m_context.framebuffer.size.width))/float(std::max(1,m_context.framebuffer.size.height));
    // Game X points right, game Y down. Highest Z is nearest, independent
    // of GLM's legacy left-handed perspective configuration.
    glm::mat4 m{1};m[0][0]=2/m_width;m[1][1]=-2*aspect/m_width;
    m[2][2]=-2/(m_z_max-m_z_min);m[3][2]=(m_z_max+m_z_min)/(m_z_max-m_z_min);return m;
  }
  const auto ratio = static_cast<float>(m_context.framebuffer.size.width) /
                     static_cast<float>(m_context.framebuffer.size.height);
  return glm::infinitePerspective(glm::radians(m_fov), ratio, m_near);
}

auto Camera::frustum() const -> geometry::Frustum {
  return geometry::Frustum{projection_matrix(), view_matrix()};
}

} // namespace rendering

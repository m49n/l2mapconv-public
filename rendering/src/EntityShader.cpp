#include "pch.h"

#include <rendering/EntityShader.h>

namespace rendering {

void EntityShader::load_projection_matrix(const glm::mat4 &projection) const {
  m_shader.load("u_projection", projection);
}

void EntityShader::load_view_matrix(const glm::mat4 &view) const {
  m_shader.load("u_view", view);
}

void EntityShader::load_model_matrix(const glm::mat4 &model) const {
  m_shader.load("u_model", model);
}

void EntityShader::load_texture_unit(int unit) const {
  m_shader.load("u_texture", unit);
}

void EntityShader::load_color(const glm::vec3 &color) const {
  m_shader.load("u_color", color);
}

void EntityShader::load_camera(const glm::vec3 &camera) const {
  m_shader.load("u_camera", camera);
}

void EntityShader::bind() const { m_shader.bind(); }
void EntityShader::unbind() const { m_shader.unbind(); }

} // namespace rendering

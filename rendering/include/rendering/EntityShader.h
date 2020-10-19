#pragma once

#include "Shader.h"

#include <utils/NonCopyable.h>

#include <glm/glm.hpp>

namespace rendering {

class EntityShader : public utils::NonCopyable {
public:
  explicit EntityShader(Shader &&shader) : m_shader{std::move(shader)} {}

  void load_projection_matrix(const glm::mat4 &projection) const;
  void load_view_matrix(const glm::mat4 &view) const;
  void load_model_matrix(const glm::mat4 &model) const;
  void load_texture_unit(int unit) const;
  void load_color(const glm::vec3 &color) const;
  void load_camera(const glm::vec3 &camera) const;

  void bind() const;
  void unbind() const;

private:
  Shader m_shader;
};

} // namespace rendering

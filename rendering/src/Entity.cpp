#include "pch.h"

#include <rendering/Entity.h>

namespace rendering {

Entity::Entity(std::shared_ptr<DrawableMesh> mesh,
               std::shared_ptr<EntityShader> shader,
               const glm::mat4 &model_matrix, bool wireframe)
    : m_mesh{std::move(mesh)}, m_shader{std::move(shader)},
      m_model_matrix{model_matrix}, m_aabb{math::Box{m_mesh->bounding_box(),
                                                     m_model_matrix}},
      m_wireframe{wireframe} {

  ASSERT(m_mesh != nullptr, "Mesh must be initialized");
  ASSERT(m_shader != nullptr, "Shader must be initialized");
}

auto Entity::mesh() const -> const std::shared_ptr<DrawableMesh> & {
  return m_mesh;
}

auto Entity::shader() const -> const std::shared_ptr<EntityShader> & {
  return m_shader;
}

auto Entity::model_matrix() const -> const glm::mat4 & {
  return m_model_matrix;
}

auto Entity::aabb() const -> const math::Box & { return m_aabb; }

auto Entity::wireframe() const -> bool { return m_wireframe; }

} // namespace rendering

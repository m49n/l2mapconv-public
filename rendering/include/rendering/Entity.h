#pragma once

#include "EntityMesh.h"
#include "EntityShader.h"
#include "Texture.h"

#include <math/Box.h>

#include <glm/glm.hpp>

#include <memory>

namespace rendering {

class Entity {
public:
  bool wireframe;

  explicit Entity(std::shared_ptr<EntityMesh> mesh,
                  std::shared_ptr<EntityShader> shader,
                  const glm::mat4 &model_matrix);

  auto mesh() const -> const std::shared_ptr<EntityMesh> &;
  auto shader() const -> const std::shared_ptr<EntityShader> &;
  auto model_matrix() const -> const glm::mat4 &;

  auto aabb() const -> const math::Box &;

private:
  std::shared_ptr<EntityMesh> m_mesh;
  std::shared_ptr<EntityShader> m_shader;
  glm::mat4 m_model_matrix;
  math::Box m_aabb;
};

} // namespace rendering

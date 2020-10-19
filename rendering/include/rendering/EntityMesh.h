#pragma once

#include "Context.h"
#include "DrawableMesh.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshSurface.h"
#include "Vertex.h"
#include "VertexBuffer.h"

#include <utils/NonCopyable.h>

#include <math/Box.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace rendering {

class EntityMesh : public utils::NonCopyable, public DrawableMesh {
public:
  explicit EntityMesh(Context &context, const std::vector<Vertex> &vertices,
                      const std::vector<std::uint32_t> &indices,
                      const std::vector<MeshSurface> &surfaces,
                      const std::vector<glm::mat4> &model_matrices,
                      const math::Box &bounding_box);

  virtual auto surfaces() const -> const std::vector<MeshSurface> & override;
  virtual auto bounding_box() const -> const math::Box & override;

  virtual void draw(const MeshSurface &surface) const override;

private:
  Mesh m_mesh;
  std::vector<MeshSurface> m_surfaces;
  std::size_t m_instances;
  math::Box m_bounding_box;

  auto vertex_buffers(const std::vector<Vertex> &vertices,
                      const std::vector<glm::mat4> &model_matrices)
      -> std::vector<VertexBuffer>;
};

} // namespace rendering

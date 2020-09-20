#pragma once

#include "Context.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshSurface.h"
#include "Vertex.h"

#include <utils/NonCopyable.h>

#include <math/Box.h>

#include <cstdint>
#include <vector>

namespace rendering {

class EntityMesh : public utils::NonCopyable {
public:
  explicit EntityMesh(Context &context, const std::vector<Vertex> &vertices,
                      const std::vector<std::uint32_t> &indices,
                      const std::vector<MeshSurface> &surfaces,
                      const math::Box &bounding_box);

  auto surfaces() const -> const std::vector<MeshSurface> &;
  auto bounding_box() const -> const math::Box &;

  void draw() const;
  void draw(const MeshSurface &surface) const;

private:
  Mesh m_mesh;
  std::vector<MeshSurface> m_surfaces;
  math::Box m_bounding_box;

  auto buffer(const std::vector<Vertex> &vertices) -> VertexBuffer;
};

} // namespace rendering

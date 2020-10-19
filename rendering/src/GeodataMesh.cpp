#include "pch.h"

#include <rendering/GeodataMesh.h>

namespace rendering {

GeodataMesh::GeodataMesh(Context &context,
                         const std::vector<GeodataBlock> &blocks,
                         const MeshSurface &surface)
    : m_mesh{context, blocks.size(), {vertex_buffer(blocks)}, {}},
      m_surfaces{surface} {

  ASSERT(!blocks.empty(), "Rendering", "Geodata must have at least one block");
}

auto GeodataMesh::surfaces() const -> const std::vector<MeshSurface> & {
  return m_surfaces;
}

auto GeodataMesh::bounding_box() const -> const math::Box & {
  return m_bounding_box;
}

void GeodataMesh::draw(const MeshSurface & /*surface*/) const {
  m_mesh.draw(GL_POINTS, 1);
}

auto GeodataMesh::vertex_buffer(const std::vector<GeodataBlock> &blocks)
    -> VertexBuffer {

  VertexBuffer vertex_buffer{blocks};
  vertex_buffer.int_layout(0, sizeof(GeodataBlock), 0);
  return vertex_buffer;
}

} // namespace rendering

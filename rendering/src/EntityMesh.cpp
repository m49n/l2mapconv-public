#include "pch.h"

#include <rendering/EntityMesh.h>
#include <rendering/VertexBuffer.h>

namespace rendering {

EntityMesh::EntityMesh(Context &context, const std::vector<Vertex> &vertices,
                       const std::vector<std::uint32_t> &indices,
                       const std::vector<MeshSurface> &surfaces,
                       const math::Box &bounding_box)
    : m_mesh{context, buffer(vertices), indices}, m_surfaces{surfaces},
      m_bounding_box{bounding_box} {

  std::size_t index_count = 0;

  for (auto &surface : m_surfaces) {
    ASSERT(index_count + surface.index_count <= m_mesh.index_count() ||
               surface.index_offset + surface.index_count <=
                   m_mesh.index_count(),
           "Rendering", "Indices out of bounds");
    ASSERT(surface.index_count > 0, "Rendering",
           "Surface must have at least one index");

    surface.m_mesh = this;
    index_count += surface.index_count;
  }

  ASSERT(index_count == m_mesh.index_count(), "Rendering",
         "Surfaces must cover entire mesh");
}

auto EntityMesh::surfaces() const -> const std::vector<MeshSurface> & {
  return m_surfaces;
}

auto EntityMesh::bounding_box() const -> const math::Box & {
  return m_bounding_box;
}

void EntityMesh::draw() const { m_mesh.draw(); }

void EntityMesh::draw(const MeshSurface &surface) const {
  ASSERT(surface.m_mesh == this, "Rendering", "Surface doesn't belong to mesh");
  m_mesh.draw(surface.index_offset, surface.index_count);
}

auto EntityMesh::buffer(const std::vector<Vertex> &vertices) -> VertexBuffer {
  VertexBuffer buffer{vertices};
  buffer.layout(0, sizeof(Vertex::position), offsetof(Vertex, position));
  buffer.layout(1, sizeof(Vertex::normal), offsetof(Vertex, normal));
  buffer.layout(2, sizeof(Vertex::uv), offsetof(Vertex, uv));
  return buffer;
}

} // namespace rendering

#include "pch.h"

#include <rendering/Mesh.h>

namespace rendering {

Mesh::Mesh(Context &context, const VertexBuffer &vertex_buffer,
           const std::vector<std::uint32_t> &indices)
    : m_context{context}, m_vao{0}, m_index_count{indices.size()} {

  ASSERT(vertex_buffer.size() != 0 && !indices.empty(), "Rendering",
         "Mesh must have at least one vertex and index");

  // Create VAO.
  GL_CALL(glGenVertexArrays(1, &m_vao));
  GL_CALL(glBindVertexArray(m_vao));

  // Create VBO.
  unsigned int vbo = 0;
  GL_CALL(glGenBuffers(1, &vbo));
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, vertex_buffer.size(),
                       vertex_buffer.data(), GL_STATIC_DRAW));

  // Attributes layout.
  for (const auto &layout : vertex_buffer.layouts()) {
    GL_CALL(glEnableVertexAttribArray(layout.index));
    GL_CALL(glVertexAttribPointer(layout.index, layout.size, layout.type,
                                  layout.normalized, layout.stride,
                                  layout.pointer));
  }

  // Create EBO.
  unsigned int ebo = 0;
  GL_CALL(glGenBuffers(1, &ebo));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
  GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                       indices.size() * sizeof(indices[0]), &indices.front(),
                       GL_STATIC_DRAW));

  // Unbind VAO for safe VBO deletion.
  GL_CALL(glBindVertexArray(0));

  // Delete buffers.
  GL_CALL(glDeleteBuffers(1, &vbo));
  GL_CALL(glDeleteBuffers(1, &ebo));
}

Mesh::~Mesh() {
  if (m_context.mesh.vao == m_vao) {
    m_context.mesh.vao = 0;
  }

  GL_CALL(glDeleteVertexArrays(1, &m_vao));
}

Mesh::Mesh(Mesh &&other) noexcept
    : m_context{other.m_context}, m_vao{other.m_vao}, m_index_count{
                                                          other.m_index_count} {

  other.m_vao = 0;
  other.m_index_count = 0;
}

auto Mesh::operator=(Mesh &&other) noexcept -> Mesh & {
  m_context = other.m_context;
  m_vao = other.m_vao;
  m_index_count = other.m_index_count;

  other.m_vao = 0;
  other.m_index_count = 0;

  return *this;
}

auto Mesh::index_count() const -> std::size_t { return m_index_count; }

void Mesh::draw() const { draw(0, m_index_count); }

void Mesh::draw(std::size_t index_offset, std::size_t index_count_) const {
  ASSERT(index_offset + index_count_ <= index_count(), "Rendering",
         "Indices out of bounds");

  if (m_context.mesh.vao != m_vao) {
    GL_CALL(glBindVertexArray(m_vao));
    m_context.mesh.vao = m_vao;
  }

  GL_CALL(glDrawElements(
      GL_TRIANGLES, index_count_, GL_UNSIGNED_INT,
      reinterpret_cast<const void *>(index_offset * sizeof(std::uint32_t))));
}

} // namespace rendering

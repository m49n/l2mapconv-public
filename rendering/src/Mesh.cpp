#include "pch.h"

#include <rendering/Mesh.h>

namespace rendering {

Mesh::Mesh(Context &context, std::size_t vertex_count,
           const std::vector<VertexBuffer> &vertex_buffers,
           const std::vector<std::uint32_t> &indices)
    : m_context{context}, m_vertex_count{vertex_count},
      m_index_count{indices.size()}, m_vao{0} {

  ASSERT(m_vertex_count != 0, "Rendering",
         "Mesh must have at least one vertex");
  ASSERT(!vertex_buffers.empty(), "Rendering",
         "Mesh must have at least one vertex buffer");

  // Create VAO.
  GL_CALL(glGenVertexArrays(1, &m_vao));
  GL_CALL(glBindVertexArray(m_vao));

  // Create VBOs.
  std::vector<unsigned int> vbos;

  for (const auto &vertex_buffer : vertex_buffers) {
    unsigned int vbo = 0;
    GL_CALL(glGenBuffers(1, &vbo));
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, vertex_buffer.size(),
                         vertex_buffer.data(), GL_STATIC_DRAW));
    vbos.push_back(vbo);

    // Float attributes layout.
    for (const auto &layout : vertex_buffer.float_layouts()) {
      GL_CALL(glEnableVertexAttribArray(layout.index));
      GL_CALL(glVertexAttribPointer(layout.index, layout.size, layout.type,
                                    layout.normalized, layout.stride,
                                    layout.pointer));
      GL_CALL(glVertexAttribDivisor(layout.index, layout.divisor));
    }

    // Int attributes layout
    for (const auto &layout : vertex_buffer.int_layouts()) {
      GL_CALL(glEnableVertexAttribArray(layout.index));
      GL_CALL(glVertexAttribIPointer(layout.index, layout.size, layout.type,
                                     layout.stride, layout.pointer));
      GL_CALL(glVertexAttribDivisor(layout.index, layout.divisor));
    }
  }

  // Create EBO.
  if (m_index_count > 0) {
    unsigned int ebo = 0;
    GL_CALL(glGenBuffers(1, &ebo));
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo));
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         indices.size() * sizeof(indices[0]), indices.data(),
                         GL_STATIC_DRAW));
    vbos.push_back(ebo);
  }

  // Unbind VAO for safe VBO deletion.
  GL_CALL(glBindVertexArray(0));

  // Delete buffers.
  GL_CALL(glDeleteBuffers(vbos.size(), vbos.data()));
}

Mesh::~Mesh() {
  if (m_context.mesh.vao == m_vao) {
    m_context.mesh.vao = 0;
  }

  GL_CALL(glDeleteVertexArrays(1, &m_vao));
}

Mesh::Mesh(Mesh &&other) noexcept
    : m_context{other.m_context}, m_vertex_count{other.m_vertex_count},
      m_index_count{other.m_index_count}, m_vao{other.m_vao} {

  other.m_vertex_count = 0;
  other.m_index_count = 0;
  other.m_vao = 0;
}

auto Mesh::operator=(Mesh &&other) noexcept -> Mesh & {
  m_context = other.m_context;
  m_vertex_count = other.m_vertex_count;
  m_index_count = other.m_index_count;
  m_vao = other.m_vao;

  other.m_index_count = 0;
  other.m_vertex_count = 0;
  other.m_vao = 0;

  return *this;
}

auto Mesh::index_count() const -> std::size_t { return m_index_count; }

void Mesh::draw(unsigned int mode, std::size_t instances,
                std::size_t index_offset, std::size_t index_count_) const {

  ASSERT(index_offset + index_count_ <= m_index_count, "Rendering",
         "Indices out of bounds");
  ASSERT(instances > 0, "Rendering",
         "Number of instances must be greater than zero");

  if (m_context.mesh.vao != m_vao) {
    GL_CALL(glBindVertexArray(m_vao));
    m_context.mesh.vao = m_vao;
  }

  if (m_index_count > 0) {
    GL_CALL(glDrawElementsInstanced(
        mode, index_count_, GL_UNSIGNED_INT,
        reinterpret_cast<const void *>(index_offset * sizeof(std::uint32_t)),
        instances));
  } else {
    GL_CALL(glDrawArraysInstanced(mode, 0, m_vertex_count, instances));
  }
}

} // namespace rendering

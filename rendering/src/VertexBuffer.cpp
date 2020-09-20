#include "pch.h"

#include <rendering/VertexBuffer.h>

namespace rendering {

auto VertexBuffer::data() const -> const void * { return m_data; }
auto VertexBuffer::size() const -> std::size_t { return m_size; }

auto VertexBuffer::layouts() const -> const std::vector<AttributeLayout> & {
  return m_layouts;
}

void VertexBuffer::layout(unsigned int index, std::size_t size,
                          std::size_t offset) {

  m_layouts.push_back({
      index,
      static_cast<int>(size / sizeof(float)),
      GL_FLOAT,
      GL_FALSE,
      m_vertex_size,
      reinterpret_cast<const void *>(offset),
  });
}

} // namespace rendering

#include "pch.h"

#include <rendering/VertexBuffer.h>

namespace rendering {

auto VertexBuffer::data() const -> const void * { return m_data; }
auto VertexBuffer::size() const -> std::size_t { return m_size; }

auto VertexBuffer::float_layouts() const
    -> const std::vector<FloatAttributeLayout> & {

  return m_float_layouts;
}

auto VertexBuffer::int_layouts() const
    -> const std::vector<IntAttributeLayout> & {

  return m_int_layouts;
}

void VertexBuffer::float_layout(unsigned int index, std::size_t size,
                                std::size_t offset, unsigned int divisor) {

  const auto count = static_cast<int>(size / sizeof(float));
  ASSERT(count > 0 && count <= 4, "Rendering",
         "Number of attribute components must be > 0 and <= 4");

  m_float_layouts.push_back({
      index,
      count,
      GL_FLOAT,
      GL_FALSE,
      m_vertex_size,
      reinterpret_cast<const void *>(offset),
      divisor,
  });
}

void VertexBuffer::int_layout(unsigned int index, std::size_t size,
                              std::size_t offset, unsigned int divisor) {

  const auto count = static_cast<int>(size / sizeof(int));
  ASSERT(count > 0 && count <= 4, "Rendering",
         "Number of attribute components must be > 0 and <= 4");

  m_int_layouts.push_back({
      index,
      count,
      GL_INT,
      m_vertex_size,
      reinterpret_cast<const void *>(offset),
      divisor,
  });
}

} // namespace rendering

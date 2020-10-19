#pragma once

#include "AttributeLayout.h"

#include <utils/Assert.h>

#include <cstdint>
#include <vector>

namespace rendering {

class VertexBuffer {
public:
  template <typename T>
  explicit VertexBuffer(const std::vector<T> &vertices)
      : m_data{vertices.data()}, m_size{vertices.size() * sizeof(T)},
        m_vertex_size{sizeof(T)} {

    ASSERT(!vertices.empty(), "Rendering",
           "Vertex buffer must have at least one vertex");
  }

  auto data() const -> const void *;
  auto size() const -> std::size_t;

  auto float_layouts() const -> const std::vector<FloatAttributeLayout> &;
  auto int_layouts() const -> const std::vector<IntAttributeLayout> &;

  void float_layout(unsigned int index, std::size_t size, std::size_t offset,
                    unsigned int divisor = 0);
  void int_layout(unsigned int index, std::size_t size, std::size_t offset,
                  unsigned int divisor = 0);

private:
  const void *m_data;
  const std::size_t m_size;
  const int m_vertex_size;
  std::vector<FloatAttributeLayout> m_float_layouts;
  std::vector<IntAttributeLayout> m_int_layouts;
};

} // namespace rendering

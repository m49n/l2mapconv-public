#pragma once

#include "AttributeLayout.h"

#include <cstdint>
#include <vector>

namespace rendering {

class VertexBuffer {
public:
  template <typename T>
  explicit VertexBuffer(const std::vector<T> &vertices)
      : m_data{&vertices.front()}, m_size{vertices.size() * sizeof(T)},
        m_vertex_size{sizeof(T)} {}

  auto data() const -> const void *;
  auto size() const -> std::size_t;
  auto layouts() const -> const std::vector<AttributeLayout> &;

  void layout(unsigned int index, std::size_t size, std::size_t offset);

private:
  const void *m_data;
  const std::size_t m_size;
  const int m_vertex_size;
  std::vector<AttributeLayout> m_layouts;
};

} // namespace rendering

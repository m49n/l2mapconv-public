#pragma once

#include "Context.h"
#include "VertexBuffer.h"

#include <utils/NonCopyable.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rendering {

class Mesh : public utils::NonCopyable {
public:
  explicit Mesh(Context &context, const VertexBuffer &vertex_buffer,
                const std::vector<std::uint32_t> &indices);

  virtual ~Mesh();

  Mesh(Mesh &&other) noexcept;
  auto operator=(Mesh &&other) noexcept -> Mesh &;

  auto index_count() const -> std::size_t;

  void draw() const;
  void draw(std::size_t index_offset, std::size_t index_count_) const;

private:
  Context &m_context;
  unsigned int m_vao;
  std::size_t m_index_count;
};

} // namespace rendering

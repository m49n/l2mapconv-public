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
  explicit Mesh(Context &context, std::size_t vertex_count,
                const std::vector<VertexBuffer> &vertex_buffers,
                const std::vector<std::uint32_t> &indices);

  virtual ~Mesh();

  Mesh(Mesh &&other) noexcept;
  auto operator=(Mesh &&other) noexcept -> Mesh &;

  auto index_count() const -> std::size_t;

  void draw(unsigned int mode, std::size_t instances,
            std::size_t index_offset = 0, std::size_t index_count_ = 0) const;

private:
  Context &m_context;
  std::size_t m_vertex_count;
  std::size_t m_index_count;
  unsigned int m_vao;
};

} // namespace rendering

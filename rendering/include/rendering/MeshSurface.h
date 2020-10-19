#pragma once

#include "Material.h"

#include <cstddef>
#include <cstdint>

namespace rendering {

class EntityMesh;

class MeshSurface {
public:
  const std::uint64_t type;
  const Material material;
  const std::size_t index_offset;
  const std::size_t index_count;

  explicit MeshSurface(std::uint64_t type, const Material &material,
                       std::size_t index_offset, std::size_t index_count)
      : type{type}, material{material}, index_offset{index_offset},
        index_count{index_count}, m_mesh{nullptr} {}

  friend class EntityMesh;

private:
  EntityMesh *m_mesh;
};

} // namespace rendering

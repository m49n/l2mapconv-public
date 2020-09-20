#pragma once

#include "Material.h"

#include <cstddef>
#include <cstdint>

namespace rendering {

class EntityMesh;

class MeshSurface {
public:
  std::uint64_t type;
  std::size_t index_offset;
  std::size_t index_count;
  Material material;

  explicit MeshSurface(std::uint64_t type, std::size_t index_offset,
                       std::size_t index_count, const Material &material)
      : type{type}, index_offset{index_offset},
        index_count{index_count}, material{material}, m_mesh{nullptr} {}

  friend class EntityMesh;

private:
  EntityMesh *m_mesh;
};

} // namespace rendering

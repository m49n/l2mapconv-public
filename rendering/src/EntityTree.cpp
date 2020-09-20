#include "pch.h"

#include <rendering/EntityTree.h>

namespace rendering {

void EntityTree::add(const Entity &entity) {
  m_entities.push_front(entity);
  const auto *inserted = &*m_entities.begin();

  for (const auto &surface : entity.mesh()->surfaces()) {
    m_tree[surface.type][entity.shader()][surface.material.texture]
          [entity.mesh()]
              .push_back({inserted, &surface});
  }
}

auto EntityTree::tree() const -> const Tree & { return m_tree; }

} // namespace rendering

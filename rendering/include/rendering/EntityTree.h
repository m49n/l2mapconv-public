#pragma once

#include "Entity.h"
#include "EntityShader.h"
#include "MeshSurface.h"
#include "Texture.h"

#include <utils/NonCopyable.h>

#include <cstdint>
#include <forward_list>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace rendering {

class EntityTree : public utils::NonCopyable {
public:
  using Tree = std::map<
      std::uint64_t,
      std::map<std::shared_ptr<const EntityShader>,
               std::map<std::shared_ptr<const Texture>,
                        std::map<std::shared_ptr<const EntityMesh>,
                                 std::vector<std::pair<
                                     const Entity *, const MeshSurface *>>>>>>;

  void add(const Entity &entity);
  auto tree() const -> const Tree &;

private:
  std::forward_list<Entity> m_entities;
  Tree m_tree;
};

} // namespace rendering

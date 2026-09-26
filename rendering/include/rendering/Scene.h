#pragma once

#include "DrawableMesh.h"
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

using SceneGroupId = std::uint64_t;

struct SceneEntry {
  SceneGroupId group;
  const Entity *entity;
  const MeshSurface *surface;
};

class Scene : public utils::NonCopyable {
public:
  // TODO: May be optimized using BVH
  using EntityEntries = std::vector<SceneEntry>;
  using MeshBranch =
      std::map<std::shared_ptr<const DrawableMesh>, EntityEntries>;
  using TextureBranch = std::map<std::shared_ptr<const Texture>, MeshBranch>;
  using ShaderBranch =
      std::map<std::shared_ptr<const EntityShader>, TextureBranch>;
  using Tree = std::map<std::uint64_t, ShaderBranch>;

  auto create_group() -> SceneGroupId;
  void add(SceneGroupId group, const Entity &entity);
  void remove(std::uint64_t surface_filter);
  void remove_group(SceneGroupId group);
  void set_group_visible(SceneGroupId group, bool visible);
  auto group_visible(SceneGroupId group) const -> bool;

  auto group_size(SceneGroupId group) const -> std::size_t;
  auto tree_entity_count() const -> std::size_t;

  auto tree() const -> const Tree &;

private:
  struct EntityRecord {
    SceneGroupId group;
    Entity entity;
  };

  std::forward_list<EntityRecord> m_entities;
  Tree m_tree;
  SceneGroupId m_next_group{1};
  std::map<SceneGroupId, bool> m_group_visibility;
};

} // namespace rendering

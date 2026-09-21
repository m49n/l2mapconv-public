#include "pch.h"

#include <rendering/Scene.h>

namespace rendering {

namespace {

template <typename Predicate>
void remove_tree_entries(Scene::Tree &tree, Predicate should_remove) {
  for (auto type = tree.begin(); type != tree.end();) {
    for (auto shader = type->second.begin(); shader != type->second.end();) {
      for (auto texture = shader->second.begin();
           texture != shader->second.end();) {
        for (auto mesh = texture->second.begin();
             mesh != texture->second.end();) {
          std::erase_if(mesh->second, should_remove);
          if (mesh->second.empty()) {
            mesh = texture->second.erase(mesh);
          } else {
            ++mesh;
          }
        }
        if (texture->second.empty()) {
          texture = shader->second.erase(texture);
        } else {
          ++texture;
        }
      }
      if (shader->second.empty()) {
        shader = type->second.erase(shader);
      } else {
        ++shader;
      }
    }
    if (type->second.empty()) {
      type = tree.erase(type);
    } else {
      ++type;
    }
  }
}

} // namespace

auto Scene::create_group() -> SceneGroupId { return m_next_group++; }

void Scene::add(SceneGroupId group, const Entity &entity) {
  ASSERT(entity.mesh() != nullptr, "Rendering", "Entity must have mesh");

  m_entities.push_front({group, entity});
  const auto *inserted = &m_entities.begin()->entity;

  for (const auto &surface : entity.mesh()->surfaces()) {
    m_tree[surface.type][entity.shader()][surface.material.texture]
          [entity.mesh()]
              .push_back({group, inserted, &surface});
  }
}

void Scene::remove(std::uint64_t surface_filter) {
  std::vector<const Entity *> removed;
  for (const auto &record : m_entities) {
    for (const auto &surface : record.entity.mesh()->surfaces()) {
      if ((surface.type & surface_filter) == surface.type) {
        removed.push_back(&record.entity);
        break;
      }
    }
  }

  const auto is_removed = [&removed](const Entity *entity) {
    return std::find(removed.begin(), removed.end(), entity) != removed.end();
  };
  remove_tree_entries(m_tree, [&is_removed](const SceneEntry &entry) {
    return is_removed(entry.entity);
  });
  m_entities.remove_if([&is_removed](const EntityRecord &record) {
    return is_removed(&record.entity);
  });
}

void Scene::remove_group(SceneGroupId group) {
  remove_tree_entries(m_tree, [group](const SceneEntry &entry) {
    return entry.group == group;
  });
  m_entities.remove_if(
      [group](const EntityRecord &record) { return record.group == group; });
}

auto Scene::group_size(SceneGroupId group) const -> std::size_t {
  return static_cast<std::size_t>(std::count_if(
      m_entities.begin(), m_entities.end(),
      [group](const EntityRecord &record) { return record.group == group; }));
}

auto Scene::tree_entity_count() const -> std::size_t {
  auto result = std::size_t{};
  for (const auto &[type, shaders] : m_tree) {
    (void)type;
    for (const auto &[shader, textures] : shaders) {
      (void)shader;
      for (const auto &[texture, meshes] : textures) {
        (void)texture;
        for (const auto &[mesh, entries] : meshes) {
          (void)mesh;
          result += entries.size();
        }
      }
    }
  }
  return result;
}

auto Scene::tree() const -> const Tree & { return m_tree; }

} // namespace rendering

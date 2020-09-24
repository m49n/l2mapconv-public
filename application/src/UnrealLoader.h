#pragma once

#include "Entity.h"

#include <unreal/Actor.h>
#include <unreal/BSP.h>
#include <unreal/Material.h>
#include <unreal/Package.h>
#include <unreal/PackageLoader.h>
#include <unreal/StaticMesh.h>
#include <unreal/Terrain.h>

#include <math/Box.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class UnrealLoader {
public:
  explicit UnrealLoader(const std::filesystem::path &root_path);

  auto load_map(const std::string &name) const -> std::vector<Entity>;

private:
  unreal::PackageLoader m_package_loader;

  mutable std::unordered_map<std::string, std::shared_ptr<Mesh>> m_mesh_cache;
  mutable std::unordered_map<std::string, std::shared_ptr<Mesh>>
      m_bb_mesh_cache;

  auto load_map_package(int x, int y) const -> std::optional<unreal::Package>;
  auto load_terrain(const unreal::Package &package) const
      -> std::shared_ptr<unreal::TerrainInfoActor>;
  auto load_side_terrain(int x, int y) const
      -> std::shared_ptr<unreal::TerrainInfoActor>;

  auto load_terrain_entities(const unreal::TerrainInfoActor &terrain) const
      -> std::vector<Entity>;
  auto load_mesh_actor_entities(const unreal::Package &package) const
      -> std::vector<Entity>;
  auto load_bsp_entities(const unreal::Package &package) const
      -> std::vector<Entity>;
  auto load_model_entity(const unreal::Model &model) const
      -> std::optional<Entity>;

  void place_actor(const unreal::Actor &actor, Entity &entity) const;

  auto collides(const unreal::StaticMeshActor &mesh_actor,
                const unreal::StaticMeshMaterial &material) const -> bool;

  auto bounding_box_mesh(std::uint64_t type, const math::Box &box) const
      -> std::shared_ptr<Mesh>;
};

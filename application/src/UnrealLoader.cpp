#include "pch.h"

#include "Conversion.h"
#include "UnrealLoader.h"

UnrealLoader::UnrealLoader(const std::filesystem::path &root_path)
    : m_package_loader{root_path,
                       {unreal::SearchConfig{"MAPS", "unr"},
                        unreal::SearchConfig{"StaticMeshes", "usx"},
                        unreal::SearchConfig{"Textures", "utx"},
                        unreal::SearchConfig{"SysTextures", "utx"}}} {}

auto UnrealLoader::load_map(const std::string &name, glm::vec3 &position) const
    -> std::vector<Entity<EntityMesh>> {

  std::vector<Entity<EntityMesh>> entities;

  const auto optional_package = m_package_loader.load_package(name);

  if (!optional_package.has_value()) {
    return {};
  }

  const auto package = optional_package.value();

  // Terrain.
  const auto terrain = load_terrain(package);

  position = to_vec3(terrain->position());
  const auto scale = to_vec3(terrain->scale());
  const auto bounding_box =
      math::Box{to_vec3(terrain->bounding_box().min) * scale + position,
                to_vec3(terrain->bounding_box().max) * scale + position};

  if (!terrain->broken_scale()) {
    const auto terrain_entities = load_terrain_entities(*terrain);
    entities.insert(entities.end(),
                    std::make_move_iterator(terrain_entities.begin()),
                    std::make_move_iterator(terrain_entities.end()));
  }

  // Mesh actors.
  const auto mesh_actor_entities = load_mesh_actor_entities(package);
  entities.insert(entities.end(),
                  std::make_move_iterator(mesh_actor_entities.begin()),
                  std::make_move_iterator(mesh_actor_entities.end()));

  // BSPs.
  const auto bsp_entities = load_bsp_entities(package, bounding_box);
  entities.insert(entities.end(), std::make_move_iterator(bsp_entities.begin()),
                  std::make_move_iterator(bsp_entities.end()));

  // Volumes.
  const auto volume_entities = load_volume_entities(package, bounding_box);
  entities.insert(entities.end(),
                  std::make_move_iterator(volume_entities.begin()),
                  std::make_move_iterator(volume_entities.end()));

  return entities;
}

auto UnrealLoader::load_map_package(int x, int y) const
    -> std::optional<unreal::Package> {

  std::stringstream stream;
  stream << x << "_" << y;
  const auto package_name = stream.str();

  const auto package = m_package_loader.load_package(package_name);
  return package;
}

auto UnrealLoader::load_terrain(const unreal::Package &package) const
    -> std::shared_ptr<unreal::TerrainInfoActor> {

  std::vector<std::shared_ptr<unreal::TerrainInfoActor>> terrains;
  package.load_objects("TerrainInfo", terrains);
  ASSERT(!terrains.empty(), "App",
         "No terrains in package: " << package.name());

  const auto &terrain = terrains[0];

  const auto &mips = terrain->terrain_map->mips;
  ASSERT(!mips.empty(), "App",
         "Can't load terrain heightmap in package: " << package.name());

  return terrain;
}

auto UnrealLoader::load_side_terrain(int x, int y) const
    -> std::shared_ptr<unreal::TerrainInfoActor> {

  const auto package = load_map_package(x, y);

  if (!package.has_value()) {
    return nullptr;
  }

  auto terrain = load_terrain(package.value());

  if (terrain->broken_scale()) {
    return nullptr;
  }

  return terrain;
}

auto UnrealLoader::load_terrain_entities(
    const unreal::TerrainInfoActor &terrain) const
    -> std::vector<Entity<EntityMesh>> {

  std::vector<Entity<EntityMesh>> entities;

  const auto &mips = terrain.terrain_map->mips;

  const auto width = terrain.terrain_map->u_size;
  const auto height = terrain.terrain_map->v_size;

  const auto mesh = std::make_shared<EntityMesh>();

  {
    const auto position = to_vec3(terrain.position());
    const auto scale = to_vec3(terrain.scale());

    const auto *heightmap = mips[0].as<std::uint16_t>();

    // Bounding box.
    const auto bounding_box = terrain.bounding_box();

    mesh->bounding_box =
        math::Box{to_vec3(bounding_box.min) * scale + position,
                  to_vec3(bounding_box.max) * scale + position};

    // Vertices.
    for (auto y = 0; y < height; ++y) {
      for (auto x = 0; x < width; ++x) {
        mesh->vertices.push_back(
            {glm::vec3{x, y, heightmap[y * width + x]} * scale + position,
             {0.0f, 0.0f, 0.0f},
             {x, y}});
      }
    }

    // Indices.
    for (auto y = 0; y < height - 1; ++y) {
      for (auto x = 0; x < width - 1; ++x) {
        if (!terrain.quad_visibility_bitmap[x + y * width]) {
          continue;
        }

        if (!terrain.edge_turn_bitmap[x + y * width]) {
          // First part of quad.
          mesh->indices.push_back(x + y * width);
          mesh->indices.push_back((x + 1) + y * width);
          mesh->indices.push_back((x + 1) + (y + 1) * width);

          // Second part of quad.
          mesh->indices.push_back(x + y * width);
          mesh->indices.push_back((x + 1) + (y + 1) * width);
          mesh->indices.push_back(x + (y + 1) * width);
        } else {
          // First part of quad.
          mesh->indices.push_back(x + (y + 1) * width);
          mesh->indices.push_back(x + y * width);
          mesh->indices.push_back((x + 1) + y * width);

          // Second part of quad.
          mesh->indices.push_back(x + (y + 1) * width);
          mesh->indices.push_back((x + 1) + y * width);
          mesh->indices.push_back((x + 1) + (y + 1) * width);
        }
      }
    }
  }

  {
    // South.
    const auto south_terrain =
        load_side_terrain(terrain.map_x, terrain.map_y + 1);

    if (south_terrain != nullptr) {
      const glm::vec3 position = {terrain.position().x, terrain.position().y,
                                  south_terrain->position().z};
      const glm::vec3 scale = {terrain.scale().x, terrain.scale().y,
                               south_terrain->scale().z};

      const auto y = height;

      const auto *heightmap =
          south_terrain->terrain_map->mips[0].as<std::uint16_t>();

      for (auto x = 0; x < width; ++x) {
        mesh->vertices.push_back(
            {glm::vec3{x, y, heightmap[x]} * scale + position,
             {0.0f, 0.0f, 0.0f},
             {x, y}});

        // First part of quad.
        if (x != width - 1) {
          mesh->indices.push_back(x + (y - 1) * width);
          mesh->indices.push_back((x + 1) + (y - 1) * width);
          mesh->indices.push_back(mesh->vertices.size() - 1);
        }

        // Second part of quad.
        if (x != 0) {
          mesh->indices.push_back(x + (y - 1) * width);
          mesh->indices.push_back(mesh->vertices.size() - 1);
          mesh->indices.push_back(mesh->vertices.size() - 2);
        }
      }
    }
  }

  {
    // East.
    const auto east_terrain =
        load_side_terrain(terrain.map_x + 1, terrain.map_y);

    if (east_terrain != nullptr) {
      const glm::vec3 position = {terrain.position().x, terrain.position().y,
                                  east_terrain->position().z};
      const glm::vec3 scale = {terrain.scale().x, terrain.scale().y,
                               east_terrain->scale().z};

      const auto x = width;

      const auto *heightmap =
          east_terrain->terrain_map->mips[0].as<std::uint16_t>();

      for (auto y = 0; y < height; ++y) {
        mesh->vertices.push_back(
            {glm::vec3{x, y, heightmap[y * width]} * scale + position,
             {0.0f, 0.0f, 0.0f},
             {x, y}});

        // First part of quad.
        if (y != height - 1) {
          mesh->indices.push_back((x - 1) + y * width);
          mesh->indices.push_back(mesh->vertices.size() - 1);
          mesh->indices.push_back((x - 1) + (y + 1) * width);
        }

        // Second part of quad.
        if (y != 0) {
          mesh->indices.push_back((x - 1) + y * width);
          mesh->indices.push_back(mesh->vertices.size() - 2);
          mesh->indices.push_back(mesh->vertices.size() - 1);
        }
      }
    }
  }

  {
    // Southeast.
    const auto southeast_terrain =
        load_side_terrain(terrain.map_x + 1, terrain.map_y + 1);

    if (southeast_terrain != nullptr) {
      const glm::vec3 position = {terrain.position().x, terrain.position().y,
                                  southeast_terrain->position().z};
      const glm::vec3 scale = {terrain.scale().x, terrain.scale().y,
                               southeast_terrain->scale().z};

      const auto x = width;
      const auto y = height;

      const auto *heightmap =
          southeast_terrain->terrain_map->mips[0].as<std::uint16_t>();

      mesh->vertices.push_back(
          {glm::vec3{x, y, heightmap[0]} * scale + position,
           {0.0f, 0.0f, 0.0f},
           {x, y}});

      // First part of quad.
      mesh->indices.push_back((x - 1) + (y - 1) * width);
      mesh->indices.push_back(mesh->vertices.size() - 2);
      mesh->indices.push_back(mesh->vertices.size() - 1);

      // Second part of quad.
      mesh->indices.push_back((x - 1) + (y - 1) * width);
      mesh->indices.push_back(mesh->vertices.size() - 1);
      mesh->indices.push_back(mesh->vertices.size() - (width + 2));
    }
  }

  // Normals.
  for (std::size_t i = 0; i < mesh->indices.size() / 3; ++i) {
    const auto index0 = mesh->indices[i * 3];
    const auto index1 = mesh->indices[i * 3 + 1];
    const auto index2 = mesh->indices[i * 3 + 2];

    const auto normal = glm::normalize(glm::cross(
        mesh->vertices[index0].position - mesh->vertices[index2].position,
        mesh->vertices[index1].position - mesh->vertices[index0].position));

    mesh->vertices[index0].normal += normal;
    mesh->vertices[index1].normal += normal;
    mesh->vertices[index2].normal += normal;
  }

  for (auto &vertex : mesh->vertices) {
    vertex.normal = glm::normalize(vertex.normal);
  }

  // Surface.
  Surface surface{};
  surface.type = SURFACE_TERRAIN;
  surface.index_offset = 0;
  surface.index_count = mesh->indices.size();
  surface.material.color = {0.85f, 0.85f, 0.85f};

  mesh->surfaces.push_back(std::move(surface));

  // Terrain entity.
  Entity entity{mesh};
  entities.push_back(std::move(entity));

  // Bounding box entity.
  Entity bb_entity{bounding_box_mesh(SURFACE_TERRAIN, mesh->bounding_box)};
  bb_entity.wireframe = true;

  entities.push_back(std::move(bb_entity));

  return entities;
}

auto UnrealLoader::load_mesh_actor_entities(
    const unreal::Package &package) const -> std::vector<Entity<EntityMesh>> {

  std::vector<Entity<EntityMesh>> entities;

  std::vector<std::shared_ptr<unreal::StaticMeshActor>> mesh_actors;
  package.load_objects("StaticMeshActor", mesh_actors);
  package.load_objects("MovableStaticMeshActor", mesh_actors);
  package.load_objects("L2MovableStaticMeshActor", mesh_actors);

  for (const auto &mesh_actor : mesh_actors) {
    if (mesh_actor->delete_me || mesh_actor->hidden) {
      continue;
    }

    auto &unreal_mesh = mesh_actor->static_mesh;

    // Rare case.
    if (!unreal_mesh) {
      continue;
    }

    const auto bounding_box = to_box(unreal_mesh->bounding_box);

    const auto &mesh_name = unreal_mesh->full_name();
    auto cached_mesh = m_mesh_cache.find(mesh_name);
    auto cached_bb_mesh = m_bb_mesh_cache.find(mesh_name);

    if (cached_bb_mesh == m_bb_mesh_cache.end()) {
      const auto bb_mesh = bounding_box_mesh(SURFACE_STATIC_MESH, bounding_box);
      cached_bb_mesh = m_bb_mesh_cache.insert({mesh_name, bb_mesh}).first;
    }

    if (cached_mesh == m_mesh_cache.end()) {
      const auto mesh = std::make_shared<EntityMesh>();
      cached_mesh = m_mesh_cache.insert({mesh_name, mesh}).first;

      // Bounding box.
      mesh->bounding_box = bounding_box;

      // Vertices.
      for (const auto &vertex : unreal_mesh->vertex_stream.vertices) {
        mesh->vertices.push_back(
            {to_vec3(vertex.location), to_vec3(vertex.normal), {0.0f, 0.0f}});
      }

      // Surfaces.
      for (auto unreal_surface = unreal_mesh->surfaces.begin(),
                end = unreal_mesh->surfaces.end();
           unreal_surface != end; ++unreal_surface) {

        // Empty surface?
        if (unreal_surface->triangle_max == 0) {
          continue;
        }

        // Indices.
        for (auto i = 0; i < unreal_surface->triangle_max; ++i) {
          mesh->indices.push_back(
              unreal_mesh->index_stream
                  .indices[unreal_surface->first_index + i * 3 + 2]);
          mesh->indices.push_back(
              unreal_mesh->index_stream
                  .indices[unreal_surface->first_index + i * 3 + 1]);
          mesh->indices.push_back(
              unreal_mesh->index_stream
                  .indices[unreal_surface->first_index + i * 3]);
        }

        // Surface.
        const auto &material = unreal_mesh->materials[std::distance(
            unreal_mesh->surfaces.begin(), unreal_surface)];

        Surface surface{};
        surface.type = SURFACE_STATIC_MESH;
        surface.index_offset = unreal_surface->first_index;
        surface.index_count = unreal_surface->triangle_max * 3;

        if (collides(*mesh_actor, material)) {
          surface.material.color = {1.0f, 0.6f, 0.6f};
        } else {
          surface.type |= SURFACE_PASSABLE;
          surface.material.color = {0.7f, 1.0f, 0.7f};
        }

        mesh->surfaces.push_back(std::move(surface));
      }
    }

    // Static mesh entity.
    Entity entity{cached_mesh->second};
    place_actor(*mesh_actor, entity);

    entities.push_back(std::move(entity));

    // Bounding box entity.
    Entity bb_entity{cached_bb_mesh->second};
    bb_entity.wireframe = true;
    place_actor(*mesh_actor, bb_entity);

    entities.push_back(std::move(bb_entity));
  }

  return entities;
}

auto UnrealLoader::load_bsp_entities(const unreal::Package &package,
                                     const math::Box &map_bounding_box) const
    -> std::vector<Entity<EntityMesh>> {

  std::vector<Entity<EntityMesh>> entities;

  std::vector<std::shared_ptr<unreal::Level>> levels;
  package.load_objects("Level", levels);
  ASSERT(!levels.empty(), "App", "No levels in package");

  for (const auto &level : levels) {
    const auto entity = load_model_entity(level->model, map_bounding_box);

    if (entity.has_value()) {
      entities.push_back(entity.value());
    }
  }

  return entities;
}

auto UnrealLoader::load_volume_entities(const unreal::Package &package,
                                        const math::Box &map_bounding_box) const
    -> std::vector<Entity<EntityMesh>> {

  std::vector<Entity<EntityMesh>> entities;

  std::vector<std::shared_ptr<unreal::BlockingVolumeActor>> volumes;
  package.load_objects("BlockingVolume", volumes);

  for (const auto &volume : volumes) {
    if (!volume->brush) {
      continue;
    }

    const auto optional_entity =
        load_model_entity(volume->brush, map_bounding_box, false);

    if (optional_entity.has_value()) {
      auto entity = optional_entity.value();
      place_actor(*volume, entity);
      entities.push_back(entity);
    }
  }

  return entities;
}

auto UnrealLoader::load_model_entity(const unreal::Model &model,
                                     const math::Box &map_bounding_box,
                                     bool check_bounds) const
    -> std::optional<Entity<EntityMesh>> {

  if (model.points.empty()) {
    return {};
  }

  const auto mesh = std::make_shared<EntityMesh>();

  for (const auto &node : model.nodes) {
    if ((node.flags & unreal::NF_Passable) != 0) {
      continue;
    }

    if (check_bounds && check_bsp_node_bounds(model, node, map_bounding_box)) {
      continue;
    }

    const auto &surface = model.surfaces[node.surface_index];

    if ((surface.polygon_flags & unreal::PF_Passable) != 0) {
      continue;
    }

    const auto &normal = model.vectors[surface.normal_index];

    const auto vertex_start = mesh->vertices.size();

    // Vertices.
    for (auto i = 0; i < node.vertex_count; ++i) {
      const auto &position =
          model.points[model.vertices[node.vertex_pool_index + i].vertex_index];

      mesh->bounding_box += to_vec3(position);

      mesh->vertices.push_back(
          {to_vec3(position), to_vec3(normal), {0.0f, 0.0f}});
    }

    if ((surface.polygon_flags & unreal::PF_TwoSided) != 0) {
      for (auto i = 0; i < node.vertex_count; ++i) {
        const auto &position =
            model.points[model.vertices[node.vertex_pool_index + i]
                             .vertex_index];

        mesh->vertices.push_back(
            {to_vec3(position), {normal.x, normal.y, -normal.z}, {0.0f, 0.0f}});
      }
    }

    // Indices.
    for (auto i = 2; i < node.vertex_count; ++i) {
      mesh->indices.push_back(vertex_start);
      mesh->indices.push_back(vertex_start + i - 1);
      mesh->indices.push_back(vertex_start + i);
    }

    if ((surface.polygon_flags & unreal::PF_TwoSided) != 0) {
      for (auto i = 2; i < node.vertex_count; ++i) {
        mesh->indices.push_back(vertex_start);
        mesh->indices.push_back(vertex_start + i);
        mesh->indices.push_back(vertex_start + i - 1);
      }
    }
  }

  if (mesh->vertices.empty()) {
    return {};
  }

  // Surface.
  Surface surface{};
  surface.type = SURFACE_CSG;
  surface.index_offset = 0;
  surface.index_count = mesh->indices.size();
  surface.material.color = {1.0f, 1.0f, 0.7f};

  mesh->surfaces.push_back(std::move(surface));

  return Entity{mesh};
}

void UnrealLoader::place_actor(const unreal::Actor &actor,
                               Entity<EntityMesh> &entity) const {

  entity.position = to_vec3(actor.position());
  entity.rotation = to_vec3(actor.rotation.vector());
  entity.scale = to_vec3(actor.scale());
}

// Reference:
// https://docs.unrealengine.com/udk/Two/StaticMeshCollisionReference.html
auto UnrealLoader::collides(const unreal::StaticMeshActor &mesh_actor,
                            const unreal::StaticMeshMaterial &material) const
    -> bool {

  if (!mesh_actor.collide_actors || !mesh_actor.block_actors ||
      !mesh_actor.block_players) {
    return false;
  }

  return material.enable_collision;
}

auto UnrealLoader::bounding_box_mesh(std::uint64_t type,
                                     const math::Box &box) const
    -> std::shared_ptr<EntityMesh> {

  const auto &min = box.min();
  const auto &max = box.max();

  auto mesh = std::make_shared<EntityMesh>();
  mesh->bounding_box = box;

  mesh->vertices.push_back(
      {{min.x, min.y, max.z}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
  mesh->vertices.push_back(
      {{max.x, min.y, max.z}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
  mesh->vertices.push_back(
      {{max.x, max.y, max.z}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
  mesh->vertices.push_back(
      {{min.x, max.y, max.z}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
  mesh->vertices.push_back(
      {{min.x, min.y, min.z}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
  mesh->vertices.push_back(
      {{max.x, min.y, min.z}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
  mesh->vertices.push_back(
      {{max.x, max.y, min.z}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});
  mesh->vertices.push_back(
      {{min.x, max.y, min.z}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}});

  mesh->indices = {// front
                   0, 1, 2, 2, 3, 0,
                   // right
                   1, 5, 6, 6, 2, 1,
                   // back
                   7, 6, 5, 5, 4, 7,
                   // left
                   4, 0, 3, 3, 7, 4,
                   // bottom
                   4, 5, 1, 1, 0, 4,
                   // top
                   3, 2, 6, 6, 7, 3};

  Surface surface{};
  surface.type = type | SURFACE_BOUNDING_BOX;
  surface.index_offset = 0;
  surface.index_count = mesh->indices.size();
  surface.material.color = {1.0f, 0.0f, 1.0f};
  mesh->surfaces.push_back(std::move(surface));

  return mesh;
}

auto UnrealLoader::check_bsp_node_bounds(
    const unreal::Model &model, const unreal::BSPNode &node,
    const math::Box &map_bounding_box) const -> bool {

  for (auto i = 0; i < node.vertex_count; ++i) {
    const auto &position =
        model.points[model.vertices[node.vertex_pool_index + i].vertex_index];

    if (!map_bounding_box.contains(to_vec3(position))) {
      return true;
    }
  }

  return false;
}

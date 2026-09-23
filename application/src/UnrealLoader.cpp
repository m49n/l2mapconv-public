#include "UnrealConverters.h"
#include "UnrealLoader.h"

#include <utils/Assert.h>
#include <utils/Log.h>

#include <unreal/Level.h>

#include <iterator>
#include <sstream>
#include <utility>

UnrealLoader::UnrealLoader(const std::filesystem::path &root_path)
    : m_package_loader{root_path,
                       {unreal::SearchConfig{"Maps", "unr"},
                        unreal::SearchConfig{"StaticMeshes", "usx"},
                        unreal::SearchConfig{"Textures", "utx"},
                        unreal::SearchConfig{"SysTextures", "utx"}}} {}

auto UnrealLoader::load_map(const std::string &name,
                            MapLoadOptions options) const -> Map {
  Map map{};

  const auto optional_package = m_package_loader.load_package(name);

  if (!optional_package.has_value()) {
    return Map{};
  }

  const auto package = optional_package.value();

  // Terrain
  const auto terrain = load_terrain(package);

  map.position = to_vec3(terrain->position());
  const auto scale = to_vec3(terrain->scale());
  map.bounding_box = geometry::Box{
      to_vec3(terrain->bounding_box().min) * scale + map.position,
      to_vec3(terrain->bounding_box().max) * scale + map.position};

#ifdef LOAD_TERRAIN
  if (options.terrain && !terrain->broken_scale()) {
    const auto terrain_entities = load_terrain_entities(*terrain);
    map.entities.insert(map.entities.end(),
                        std::make_move_iterator(terrain_entities.begin()),
                        std::make_move_iterator(terrain_entities.end()));
  }
#endif

  // Mesh actors
  if (options.static_meshes) {
    const auto mesh_actor_entities = load_mesh_actor_entities(package);
    map.entities.insert(map.entities.end(),
                        std::make_move_iterator(mesh_actor_entities.begin()),
                        std::make_move_iterator(mesh_actor_entities.end()));
  }

  // BSPs
  if (options.csg) {
    const auto bsp_entities = load_bsp_entities(package, map.bounding_box);
    map.entities.insert(map.entities.end(),
                        std::make_move_iterator(bsp_entities.begin()),
                        std::make_move_iterator(bsp_entities.end()));
  }

  // Volumes
  if (options.blocking_volumes) {
    const auto volume_entities =
        load_volume_entities(package, map.bounding_box);
    map.entities.insert(map.entities.end(),
                        std::make_move_iterator(volume_entities.begin()),
                        std::make_move_iterator(volume_entities.end()));
  }

  return map;
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

  // Size with edges
  const auto full_width = width + 1;
  const auto full_height = height + 1;

  const auto mesh = std::make_shared<EntityMesh>();

  std::vector<std::uint16_t> heights(full_width * full_height);

  {
    const auto position = to_vec3(terrain.position());
    const auto scale = to_vec3(terrain.scale());

    const auto *heightmap = mips[0].as<std::uint16_t>();

    // Bounding box
    const auto bounding_box = terrain.bounding_box();

    mesh->bounding_box =
        geometry::Box{to_vec3(bounding_box.min) * scale + position,
                      to_vec3(bounding_box.max) * scale + position};

    // Vertices
    for (auto y = 0; y < height; ++y) {
      for (auto x = 0; x < width; ++x) {
        mesh->vertices.push_back(
            {glm::vec3{x, y, heightmap[y * width + x]} * scale + position,
             {0.0f, 0.0f, 0.0f},
             {0.0f, 0.0f}});

        heights[y * full_width + x] = heightmap[y * width + x];
      }
    }

    // Indices
    for (auto y = 0; y < height - 1; ++y) {
      for (auto x = 0; x < width - 1; ++x) {
        if (!terrain.quad_visibility_bitmap[x + y * width]) {
          continue;
        }

        if (!terrain.edge_turn_bitmap[x + y * width]) {
          // First part of quad
          mesh->indices.push_back((x + 0) + (y + 0) * width);
          mesh->indices.push_back((x + 1) + (y + 0) * width);
          mesh->indices.push_back((x + 1) + (y + 1) * width);

          // Second part of quad
          mesh->indices.push_back((x + 0) + (y + 0) * width);
          mesh->indices.push_back((x + 1) + (y + 1) * width);
          mesh->indices.push_back((x + 0) + (y + 1) * width);
        } else {
          // First part of quad
          mesh->indices.push_back((x + 0) + (y + 1) * width);
          mesh->indices.push_back((x + 0) + (y + 0) * width);
          mesh->indices.push_back((x + 1) + (y + 0) * width);

          // Second part of quad
          mesh->indices.push_back((x + 0) + (y + 1) * width);
          mesh->indices.push_back((x + 1) + (y + 0) * width);
          mesh->indices.push_back((x + 1) + (y + 1) * width);
        }
      }
    }
  }

  // South
  const auto south_terrain =
      load_side_terrain(terrain.map_x, terrain.map_y + 1);

  {
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
             {0.0f, 0.0f}});

        heights[y * full_width + x] = heightmap[x];

        // First part of quad
        if (x != width - 1) {
          mesh->indices.push_back((x + 0) + (y - 1) * width);
          mesh->indices.push_back((x + 1) + (y - 1) * width);
          mesh->indices.push_back(mesh->vertices.size() - 1);
        }

        // Second part of quad
        if (x != 0) {
          mesh->indices.push_back((x + 0) + (y - 1) * width);
          mesh->indices.push_back(mesh->vertices.size() - 1);
          mesh->indices.push_back(mesh->vertices.size() - 2);
        }
      }
    }
  }

  // East
  const auto east_terrain = load_side_terrain(terrain.map_x + 1, terrain.map_y);

  {
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
             {0.0f, 0.0f}});

        heights[y * full_width + x] = heightmap[y * width];

        // First part of quad
        if (y != height - 1) {
          mesh->indices.push_back((x - 1) + (y + 0) * width);
          mesh->indices.push_back(mesh->vertices.size() - 1);
          mesh->indices.push_back((x - 1) + (y + 1) * width);
        }

        // Second part of quad
        if (y != 0) {
          mesh->indices.push_back((x - 1) + (y + 0) * width);
          mesh->indices.push_back(mesh->vertices.size() - 2);
          mesh->indices.push_back(mesh->vertices.size() - 1);
        }
      }
    }
  }

  // Southeast
  const auto southeast_terrain =
      load_side_terrain(terrain.map_x + 1, terrain.map_y + 1);

  {
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
           {0.0f, 0.0f}});

      heights[y * full_width + x] = heightmap[0];

      // First part of quad
      mesh->indices.push_back((x - 1) + (y - 1) * width);
      mesh->indices.push_back(mesh->vertices.size() - 2);
      mesh->indices.push_back(mesh->vertices.size() - 1);

      // Second part of quad
      mesh->indices.push_back((x - 1) + (y - 1) * width);
      mesh->indices.push_back(mesh->vertices.size() - 1);
      mesh->indices.push_back(mesh->vertices.size() - (width + 2));
    }
  }

  {
    // Normals
    for (auto y = 0; y < full_height; ++y) {
      for (auto x = 0; x < full_width; ++x) {
        const float z = heights[y * full_width + x];

        const auto top = y > 0 ? heights[(y - 1) * full_width + x] : z;
        const auto bottom = y < height - (south_terrain == nullptr ? 1 : 0)
                                ? heights[(y + 1) * full_width + x]
                                : z;
        const auto left = x > 0 ? heights[y * full_width + (x - 1)] : z;
        const auto right = x < width - (east_terrain == nullptr ? 1 : 0)
                               ? heights[y * full_width + (x + 1)]
                               : z;

        const auto normal = glm::normalize(
            glm::vec3{(left - right) / (full_width * 2.0f),
                      (top - bottom) / (full_height * 2.0f), 4.0f});

        if (x < width && y < height) {
          mesh->vertices[y * width + x].normal = normal;
        } else if (x == width && y == height && southeast_terrain != nullptr) {
          // Southeast
          mesh->vertices[y * full_width + x].normal = normal;
        } else if (y == height && south_terrain != nullptr) {
          // South
          mesh->vertices[y * width + x].normal = normal;
        } else if (x == width && east_terrain != nullptr) {
          // East
          mesh->vertices[full_height * width + y].normal = normal;
        }
      }
    }
  }

  // Surface
  Surface surface{};
  surface.type = SURFACE_TERRAIN;
  surface.index_offset = 0;
  surface.index_count = mesh->indices.size();
  surface.material.color = {0.85f, 0.85f, 0.85f};

  mesh->surfaces.push_back(surface);

  // Terrain entity
  Entity entity{mesh};
  entities.push_back(std::move(entity));

  // Bounding box entity
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

    const auto &unreal_mesh = mesh_actor->static_mesh;

    if (!unreal_mesh) {
      utils::Log(utils::LOG_WARN, "App")
          << "No static mesh for actor: " << mesh_actor->full_name()
          << std::endl;
      continue;
    }

    const auto bounding_box = to_box(unreal_mesh->bounding_box);

    const auto &mesh_name = unreal_mesh->full_name();
    const auto actor_collides = mesh_actor->collide_actors &&
                                mesh_actor->block_actors &&
                                mesh_actor->block_players;
    auto &mesh_cache = m_mesh_cache[actor_collides ? 1 : 0];
    auto cached_mesh = mesh_cache.find(mesh_name);
    auto cached_bb_mesh = m_bb_mesh_cache.find(mesh_name);

    if (cached_bb_mesh == m_bb_mesh_cache.end()) {
      const auto bb_mesh = bounding_box_mesh(SURFACE_STATIC_MESH, bounding_box);
      cached_bb_mesh = m_bb_mesh_cache.insert({mesh_name, bb_mesh}).first;
    }

    if (cached_mesh == mesh_cache.end()) {
      const auto mesh = std::make_shared<EntityMesh>();
      cached_mesh = mesh_cache.insert({mesh_name, mesh}).first;

      // Bounding box
      mesh->bounding_box = bounding_box;

      // Vertices
      const auto &vertices = unreal_mesh->vertex_stream.vertices;
      for (std::size_t vertex_index = 0; vertex_index < vertices.size();
           ++vertex_index) {
        const auto &vertex = vertices[vertex_index];
        mesh->vertices.push_back({to_vec3(vertex.location),
                                  to_vec3(vertex.normal),
                                  primary_uv(unreal_mesh->uv_stream,
                                             vertex_index)});
      }

      // Surfaces
      for (auto unreal_surface = unreal_mesh->surfaces.begin(),
                end = unreal_mesh->surfaces.end();
           unreal_surface != end; ++unreal_surface) {

        // Empty surface?
        if (unreal_surface->triangle_max == 0) {
          continue;
        }

        // Indices
        for (auto i = 0; i < unreal_surface->triangle_max; ++i) {
          const auto *indices =
              &unreal_mesh->index_stream
                   .indices[unreal_surface->first_index + i * 3];

          mesh->indices.push_back(indices[2]);
          mesh->indices.push_back(indices[1]);
          mesh->indices.push_back(indices[0]);
        }

        // Surface
        const auto &unreal_material = unreal_mesh->materials[std::distance(
            unreal_mesh->surfaces.begin(), unreal_surface)];

        Surface surface{};
        surface.type = SURFACE_STATIC_MESH;
        surface.index_offset = unreal_surface->first_index;
        surface.index_count = unreal_surface->triangle_max * 3;

        if (collides(*mesh_actor, unreal_material)) {
          surface.material.color = {1.0f, 0.6f, 0.6f};
        } else {
          surface.type |= SURFACE_PASSABLE;
          surface.material.color = {0.7f, 1.0f, 0.7f};
        }

#ifdef LOAD_TEXTURES
        if (const auto material = load_material(unreal_material.material)) {
          surface.material.texture = material->texture;
        }
#endif

        mesh->surfaces.push_back(surface);
      }
    }

    // Static mesh entity
    Entity entity{cached_mesh->second};
    place_actor(*mesh_actor, entity);
    entities.push_back(std::move(entity));

    // Bounding box entity
    Entity bb_entity{cached_bb_mesh->second};
    bb_entity.wireframe = true;
    place_actor(*mesh_actor, bb_entity);
    entities.push_back(std::move(bb_entity));
  }

  return entities;
}

auto UnrealLoader::load_bsp_entities(
    const unreal::Package &package, const geometry::Box &map_bounding_box) const
    -> std::vector<Entity<EntityMesh>> {

  std::vector<Entity<EntityMesh>> entities;

  std::vector<std::shared_ptr<unreal::Level>> levels;
  package.load_objects("Level", levels);
  ASSERT(!levels.empty(), "App", "No levels in package");

  for (const auto &level : levels) {
    if (const auto entity = load_model_entity(level->model, map_bounding_box)) {
      entities.push_back(*entity);
    }
  }

  return entities;
}

auto UnrealLoader::load_volume_entities(
    const unreal::Package &package, const geometry::Box &map_bounding_box) const
    -> std::vector<Entity<EntityMesh>> {

  std::vector<Entity<EntityMesh>> entities;

  std::vector<std::shared_ptr<unreal::VolumeActor>> volumes;
  package.load_objects("BlockingVolume", volumes);
  //  package.load_objects("WaterVolume", volumes);

  for (const auto &volume : volumes) {
    if (!volume->brush) {
      continue;
    }

    if (auto entity = load_model_entity(volume->brush, map_bounding_box, false,
                                        SURFACE_BLOCKING_VOLUME)) {
      place_actor(*volume, *entity);
      entities.push_back(*entity);
    }
  }

  return entities;
}

auto UnrealLoader::load_model_entity(const unreal::Model &model,
                                     const geometry::Box &map_bounding_box,
                                     bool check_bounds,
                                     std::uint64_t surface_type) const
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

    const auto &unreal_surface = model.surfaces[node.surface_index];

    if ((unreal_surface.polygon_flags & unreal::PF_Passable) != 0) {
      continue;
    }

    const auto &normal = model.vectors[unreal_surface.normal_index];
    const auto vertex_offset = mesh->vertices.size();
    const auto index_offset = mesh->indices.size();

    // UVs
    const auto u_vector = to_vec3(model.vectors[unreal_surface.u_index]);
    const auto v_vector = to_vec3(model.vectors[unreal_surface.v_index]);
    const auto base = to_vec3(model.points[unreal_surface.base_index]);

#ifdef LOAD_TEXTURES
    const auto material = load_material(unreal_surface.material);
#endif

    auto u_size = 64.0f;
    auto v_size = 64.0f;

#ifdef LOAD_TEXTURES
    if (material) {
      u_size = material->texture.width;
      v_size = material->texture.height;
    }
#endif

    // Vertices
    for (auto i = 0; i < node.vertex_count; ++i) {
      const auto index =
          model.vertices[node.vertex_pool_index + i].vertex_index;
      const auto position = to_vec3(model.points[index]);

      const auto distance = position - base;
      const auto texture = glm::vec2{glm::dot(distance, u_vector) / u_size,
                                     glm::dot(distance, v_vector) / v_size};

      mesh->bounding_box += position;

      mesh->vertices.push_back(
          {position, to_vec3(normal), {texture.x, texture.y}});
    }

    if ((unreal_surface.polygon_flags & unreal::PF_TwoSided) != 0) {
      for (auto i = 0; i < node.vertex_count; ++i) {
        const auto index =
            model.vertices[node.vertex_pool_index + i].vertex_index;
        const auto position = to_vec3(model.points[index]);

        const auto distance = position - base;
        const auto texture = glm::vec2{glm::dot(distance, u_vector) / u_size,
                                       glm::dot(distance, v_vector) / v_size};

        mesh->vertices.push_back({position,
                                  {normal.x, normal.y, -normal.z},
                                  {texture.x, texture.y}});
      }
    }

    // Indices
    for (auto i = 2; i < node.vertex_count; ++i) {
      mesh->indices.push_back(vertex_offset);
      mesh->indices.push_back(vertex_offset + i - 1);
      mesh->indices.push_back(vertex_offset + i);
    }

    if ((unreal_surface.polygon_flags & unreal::PF_TwoSided) != 0) {
      for (auto i = 2; i < node.vertex_count; ++i) {
        mesh->indices.push_back(vertex_offset);
        mesh->indices.push_back(vertex_offset + i);
        mesh->indices.push_back(vertex_offset + i - 1);
      }
    }

    // Surface
    Surface surface{};
    surface.type = surface_type;
    surface.index_offset = index_offset;
    surface.index_count = mesh->indices.size() - index_offset;
    surface.material.color = {1.0f, 1.0f, 0.7f};

#ifdef LOAD_TEXTURES
    if (material) {
      surface.material.texture = material->texture;
    }
#endif

    mesh->surfaces.push_back(surface);
  }

  if (mesh->vertices.empty()) {
    return {};
  }

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
                                     const geometry::Box &box) const
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

  mesh->indices = {
      0, 1, 2, 2, 3, 0, // front
      1, 5, 6, 6, 2, 1, // right
      7, 6, 5, 5, 4, 7, // back
      4, 0, 3, 3, 7, 4, // left
      4, 5, 1, 1, 0, 4, // botton
      3, 2, 6, 6, 7, 3, // top
  };

  Surface surface{};
  surface.type = type | SURFACE_BOUNDING_BOX;
  surface.index_offset = 0;
  surface.index_count = mesh->indices.size();
  surface.material.color = {1.0f, 0.0f, 1.0f};
  mesh->surfaces.push_back(surface);

  return mesh;
}

auto UnrealLoader::check_bsp_node_bounds(
    const unreal::Model &model, const unreal::BSPNode &node,
    const geometry::Box &map_bounding_box) const -> bool {

  for (auto i = 0; i < node.vertex_count; ++i) {
    const auto &position =
        model.points[model.vertices[node.vertex_pool_index + i].vertex_index];

    if (!map_bounding_box.contains(to_vec3(position))) {
      return true;
    }
  }

  return false;
}

auto UnrealLoader::load_material(
    std::shared_ptr<unreal::Material> unreal_material) const
    -> std::optional<Material> {

  if (unreal_material == nullptr) {
    return {};
  }

  Material material{};

  if (std::shared_ptr<unreal::Texture> unreal_texture =
          std::dynamic_pointer_cast<unreal::Texture>(unreal_material)) {

    if (const auto texture = load_texture(unreal_texture)) {
      material.texture = *texture;
      return material;
    }
  } else if (std::shared_ptr<unreal::Shader> unreal_shader =
                 std::dynamic_pointer_cast<unreal::Shader>(unreal_material)) {

    if (const auto texture = load_texture(unreal_shader->diffuse.as<unreal::Texture>())) {
      material.texture = *texture;
      return material;
    }
  } else if (std::shared_ptr<unreal::FinalBlend> unreal_final_blend =
                 std::dynamic_pointer_cast<unreal::FinalBlend>(
                     unreal_material)) {

    return load_material(unreal_final_blend->material);
  } else if (std::shared_ptr<unreal::Modifier> unreal_modifier =
                 std::dynamic_pointer_cast<unreal::Modifier>(unreal_material)) {

    return load_material(unreal_modifier->material);
  } else if (std::shared_ptr<unreal::Combiner> unreal_combiner =
                 std::dynamic_pointer_cast<unreal::Combiner>(unreal_material)) {

    return load_material(unreal_combiner->material1);
  }

  return {};
}

auto UnrealLoader::load_texture(std::shared_ptr<unreal::Texture> unreal_texture)
    const -> std::optional<Texture> {

  if (unreal_texture == nullptr) {
    return {};
  }

  if (unreal_texture->mips.empty()) {
    return {};
  }

  TextureFormat format = TEXTURE_DXT1;

  switch (unreal_texture->format) {
  case unreal::TextureFormat::TEXF_DXT1: {
    format = TEXTURE_DXT1;
  } break;
  case unreal::TextureFormat::TEXF_DXT3: {
    format = TEXTURE_DXT3;
  } break;
  case unreal::TextureFormat::TEXF_DXT5: {
    format = TEXTURE_DXT5;
  } break;
  case unreal::TextureFormat::TEXF_RGBA8: {
    format = TEXTURE_RGBA;
  } break;
  default: {
    utils::Log(utils::LOG_WARN, "App")
        << "Unknown texture format: "
        << static_cast<int>(unreal_texture->format) << std::endl;
    return {};
  }
  }

  const auto *mip = &unreal_texture->mips[0];

  return Texture{
      format,
      mip->u_size,
      mip->v_size,
      mip->data.data(),
  };
}

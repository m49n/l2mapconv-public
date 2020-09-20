#include "pch.h"

#include "UnrealLoader.h"

// TODO: names, add comments,

UnrealLoader::UnrealLoader(const std::filesystem::path &root_path)
    : m_package_loader{root_path,
                       {unreal::SearchConfig{"MAPS", "unr"},
                        unreal::SearchConfig{"StaticMeshes", "usx"},
                        unreal::SearchConfig{"Textures", "utx"},
                        unreal::SearchConfig{"SysTextures", "utx"}}} {}

auto UnrealLoader::load_map(const std::string &name) const
    -> std::vector<Entity> {

  std::vector<Entity> entities;

  const auto package = m_package_loader.load_package(name).value();

  // Terrains.
  const auto terrains = load_terrains(package);
  entities.insert(entities.end(), std::make_move_iterator(terrains.begin()),
                  std::make_move_iterator(terrains.end()));

  // Mesh actors.
  const auto mesh_actors = load_mesh_actors(package);
  entities.insert(entities.end(), std::make_move_iterator(mesh_actors.begin()),
                  std::make_move_iterator(mesh_actors.end()));

  // BSPs.
  const auto bsps = load_bsps(package);
  entities.insert(entities.end(), std::make_move_iterator(bsps.begin()),
                  std::make_move_iterator(bsps.end()));

  return entities;
}

auto UnrealLoader::load_terrains(const unreal::Package &package) const
    -> std::vector<Entity> {

  std::vector<Entity> entities;

  std::vector<std::shared_ptr<unreal::TerrainInfoActor>> terrains;
  package.load_objects("TerrainInfo", terrains);
  ASSERT(!terrains.empty(), "App",
         "No terrains in package: " << package.name());

  for (const auto &terrain : terrains) {
    if (terrain->layers.empty()) {
      utils::Log(utils::LOG_WARN, "App")
          << "Empty terrain layers in package: " << package.name() << std::endl;
      continue;
    }

    const auto &mips = terrain->terrain_map->mips;
    ASSERT(!mips.empty(), "App",
           "Can't load terrain height map in package: " << package.name());

    const auto *height_map = mips[0].as<std::uint16_t>();

    const auto width = terrain->terrain_map->u_size;
    const auto height = terrain->terrain_map->v_size;

    const auto mesh = std::make_shared<Mesh>();

    // Bounding box.
    const auto &bounding_box = terrain->bounding_box();

    mesh->bounding_box =
        math::Box{{bounding_box.min.x, bounding_box.min.y, bounding_box.min.z},
                  {bounding_box.max.x, bounding_box.max.y, bounding_box.max.z}};

    // Vertices.
    for (auto y = 0; y < height; ++y) {
      for (auto x = 0; x < width; ++x) {
        mesh->vertices.push_back(
            {{x, y, height_map[y * width + x]}, {0.0f, 0.0f, 0.0f}, {x, y}});
      }
    }

    // Indices.
    for (auto y = 0; y < height - 1; ++y) {
      for (auto x = 0; x < width - 1; ++x) {
        if (!terrain->quad_visibility_bitmap[x + y * width]) {
          continue;
        }

        if (!terrain->edge_turn_bitmap[x + y * width]) {
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
    surface.material.color = {1.0f, 1.0f, 1.0f};

    mesh->surfaces.push_back(std::move(surface));

    // Terrain entity.
    const auto position = terrain->position();
    const auto scale = terrain->scale();

    Entity entity{mesh};
    entity.position = {position.x, position.y, position.z};
    entity.scale = {scale.x, scale.y, scale.z};

    entities.push_back(entity);

    // Bounding box entity.
    Entity bb_entity{bounding_box_mesh(SURFACE_TERRAIN, mesh->bounding_box)};
    bb_entity.wireframe = true;
    bb_entity.position = entity.position;
    bb_entity.scale = entity.scale;

    entities.push_back(bb_entity);
  }

  return entities;
}

auto UnrealLoader::load_mesh_actors(const unreal::Package &package) const
    -> std::vector<Entity> {

  std::vector<Entity> entities;

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

    const auto &bounding_box = math::Box{
        {unreal_mesh->bounding_box.min.x, unreal_mesh->bounding_box.min.y,
         unreal_mesh->bounding_box.min.z},
        {unreal_mesh->bounding_box.max.x, unreal_mesh->bounding_box.max.y,
         unreal_mesh->bounding_box.max.z}};

    const auto &mesh_name = unreal_mesh->full_name();
    auto cached_mesh = m_mesh_cache.find(mesh_name);
    auto cached_bb_mesh = m_bb_mesh_cache.find(mesh_name);

    if (cached_bb_mesh == m_bb_mesh_cache.end()) {
      const auto bb_mesh = bounding_box_mesh(SURFACE_STATIC_MESH, bounding_box);
      cached_bb_mesh = m_bb_mesh_cache.insert({mesh_name, bb_mesh}).first;
    }

    if (cached_mesh == m_mesh_cache.end()) {
      const auto mesh = std::make_shared<Mesh>();
      cached_mesh = m_mesh_cache.insert({mesh_name, mesh}).first;

      // Bounding box.
      mesh->bounding_box = bounding_box;

      // Vertices.
      for (const auto &vertex : unreal_mesh->vertex_stream.vertices) {
        mesh->vertices.push_back(
            {{vertex.location.x, vertex.location.y, vertex.location.z},
             {vertex.normal.x, vertex.normal.y, vertex.normal.z},
             {0.0f, 0.0f}});
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
          surface.material.color = glm::vec3{1.0f, 0.6f, 0.6f};
        } else {
          surface.type |= SURFACE_PASSABLE;
          surface.material.color = glm::vec3{0.7f, 1.0f, 0.7f};
        }

        mesh->surfaces.push_back(std::move(surface));
      }
    }

    // Static mesh entity.
    Entity entity{cached_mesh->second};
    place_actor(*mesh_actor, entity);

    if (entity.position == glm::vec3{0.0f}) {
      continue;
    }

    entities.push_back(entity);

    // Bounding box entity.
    Entity bb_entity{cached_bb_mesh->second};
    bb_entity.wireframe = true;
    place_actor(*mesh_actor, bb_entity);

    entities.push_back(bb_entity);
  }

  return entities;
}

auto UnrealLoader::load_bsps(const unreal::Package &package) const
    -> std::vector<Entity> {

  std::vector<Entity> entities;

  std::vector<std::shared_ptr<unreal::Level>> levels;
  package.load_objects("Level", levels);
  ASSERT(!levels.empty(), "App", "No levels in package");

  for (const auto &level : levels) {
    if (!level->model->points.empty()) {
      entities.push_back(load_model(level->model));
    }
  }

  return entities;
}

auto UnrealLoader::load_model(const unreal::Model &model) const -> Entity {
  const auto mesh = std::make_shared<Mesh>();

  for (const auto &node : model.nodes) {
    const auto &surface = model.surfaces[node.surface_index];

    if ((surface.polygon_flags & unreal::PF_Passable) != 0 ||
        !surface.material) {
      continue;
    }

    const auto &normal = model.vectors[surface.normal_index];

    const auto vertex_start = mesh->vertices.size();

    // Vertices.
    for (auto i = 0; i < node.vertex_count; ++i) {
      const auto &position =
          model.points[model.vertices[node.vertex_pool_index + i].vertex_index];

      mesh->bounding_box += {position.x, position.y, position.z};

      mesh->vertices.push_back({{position.x, position.y, position.z},
                                {normal.x, normal.y, normal.z},
                                {0.0f, 0.0f}});
    }

    if ((surface.polygon_flags & unreal::PF_TwoSided) != 0) {
      for (auto i = 0; i < node.vertex_count; ++i) {
        const auto &position =
            model.points[model.vertices[node.vertex_pool_index + i]
                             .vertex_index];

        mesh->vertices.push_back({{position.x, position.y, position.z},
                                  {normal.x, normal.y, -normal.z},
                                  {0.0f, 0.0f}});
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
                               Entity &entity) const {

  const auto position = actor.position();
  const auto rotation = actor.rotation.vector();
  const auto scale = actor.scale();

  entity.position = glm::vec3{position.x, position.y, position.z};
  entity.rotation = {rotation.x, rotation.y, rotation.z};
  entity.scale = glm::vec3{scale.x, scale.y, scale.z};
}

// Reference:
// https://docs.unrealengine.com/udk/Two/StaticMeshCollisionReference.html
auto UnrealLoader::collides(const unreal::StaticMeshActor &mesh_actor,
                            const unreal::StaticMeshMaterial &material) const
    -> bool {

  if (!mesh_actor.collide_actors) {
    return false;
  }

  return material.enable_collision || mesh_actor.static_mesh->collision_model;
}

auto UnrealLoader::bounding_box_mesh(std::uint64_t type,
                                     const math::Box &box) const
    -> std::shared_ptr<Mesh> {

  const auto &min = box.min();
  const auto &max = box.max();

  auto mesh = std::make_shared<Mesh>();
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
  mesh->surfaces.push_back(surface);

  return mesh;
}

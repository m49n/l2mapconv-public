#include "pch.h"

#include "GeodataLoader.h"
#include "LoadingSystem.h"
#include "UnrealLoader.h"

LoadingSystem::LoadingSystem(RenderingContext &rendering_context,
                             const std::filesystem::path &root_path,
                             const std::vector<std::string> &maps)
    : m_rendering_context{rendering_context} {

  UnrealLoader unreal_loader{root_path};
  GeodataLoader geodata_loader;

  rendering::ShaderLoader shader_loader{m_rendering_context.context, "shaders"};
  const auto entity_shader = shader_loader.load_entity_shader("entity");
  const auto geodata_shader = shader_loader.load_entity_shader("geodata");

  rendering::TextureLoader texture_loader{m_rendering_context.context,
                                          "textures"};
  const auto nswe_texture = texture_loader.load_texture("nswe.png");

  std::unordered_map<std::shared_ptr<EntityMesh>,
                     std::shared_ptr<rendering::EntityMesh>>
      mesh_cache;

  for (const auto &map : maps) {
    glm::vec3 map_position{};

    const auto map_entities = unreal_loader.load_map(map, map_position);

    // Set initial camera position.
    if (!map_entities.empty()) {
      m_rendering_context.camera.set_position(
          {map_position.x + 256.0f * 64.0f, map_position.y, 0.0f});
    }

    for (const auto &entity : map_entities) {
      auto cached_mesh = mesh_cache.find(entity.mesh);

      if (cached_mesh == mesh_cache.end()) {
        std::vector<rendering::MeshSurface> surfaces;

        for (const auto &surface : entity.mesh->surfaces) {
          surfaces.emplace_back(
              surface.type,
              rendering::Material{surface.material.color, nullptr},
              surface.index_offset, surface.index_count);
        }

        std::vector<rendering::Vertex> vertices;

        for (const auto &vertex : entity.mesh->vertices) {
          vertices.push_back({vertex.position, vertex.normal, vertex.uv});
        }

        const std::vector<glm::mat4> &model_matrices =
            entity.mesh->model_matrices.empty() ? std::vector{glm::mat4{1.0f}}
                                                : entity.mesh->model_matrices;

        const auto mesh = std::make_shared<rendering::EntityMesh>(
            m_rendering_context.context, vertices, entity.mesh->indices,
            surfaces, model_matrices, entity.mesh->bounding_box);

        cached_mesh = mesh_cache.insert({entity.mesh, mesh}).first;
      }

      rendering::Entity rendering_entity{cached_mesh->second, entity_shader,
                                         entity.model_matrix(),
                                         entity.wireframe};

      m_rendering_context.entity_tree.add(rendering_entity);
    }

    const auto geodata_entities =
        geodata_loader.load_geodata(map, map_position);

    for (const auto &entity : geodata_entities) {
      std::vector<rendering::GeodataBlock> blocks;

      for (const auto &block : entity.mesh->blocks) {
        blocks.push_back({
            static_cast<std::int32_t>(block.x),
            static_cast<std::int32_t>(block.y),
            static_cast<std::int32_t>(block.z),
            static_cast<std::uint16_t>(block.type),
            block.north,
            block.south,
            block.west,
            block.east,
        });
      }

      rendering::MeshSurface surface{
          entity.mesh->surface.type,
          rendering::Material{entity.mesh->surface.material.color,
                              nswe_texture},
          0, 0};

      const auto mesh = std::make_shared<rendering::GeodataMesh>(
          m_rendering_context.context, blocks, surface);

      rendering::Entity rendering_entity{mesh, geodata_shader,
                                         entity.model_matrix(), false};

      m_rendering_context.entity_tree.add(rendering_entity);
    }
  }
}

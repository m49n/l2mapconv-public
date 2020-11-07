#include "pch.h"

#include "GeodataLoader.h"
#include "LoadingSystem.h"
#include "UnrealLoader.h"

LoadingSystem::LoadingSystem(RenderingContext &rendering_context,
                             const std::filesystem::path &root_path,
                             const std::vector<std::string> &map_names)
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
      entity_mesh_cache;

  std::unordered_map<const unsigned char *, std::shared_ptr<rendering::Texture>>
      texture_cache;

  for (const auto &map_name : map_names) {
    // Load map entities.
    const auto map = unreal_loader.load_map(map_name);

    // Set initial camera position.
    if (!map.entities.empty()) {
      m_rendering_context.camera.set_position(
          {map.position.x + 256.0f * 64.0f, map.position.y, 0.0f});
    }

    for (const auto &entity : map.entities) {
      // Load mesh if needed.
      auto cached_mesh = entity_mesh_cache.find(entity.mesh);

      if (cached_mesh == entity_mesh_cache.end()) {
        std::vector<rendering::MeshSurface> surfaces;

        for (const auto &surface : entity.mesh->surfaces) {
          // Load texture if needed.
          auto cached_texture =
              texture_cache.find(surface.material.texture.data);

          if (cached_texture == texture_cache.end()) {
            const auto texture = load_texture(surface.material.texture);
            cached_texture =
                texture_cache.insert({surface.material.texture.data, texture})
                    .first;
          }

          // Add surface.
          surfaces.emplace_back(surface.type,
                                rendering::Material{surface.material.color,
                                                    cached_texture->second},
                                surface.index_offset, surface.index_count);
        }

        // Add vertices.
        std::vector<rendering::Vertex> vertices;

        for (const auto &vertex : entity.mesh->vertices) {
          vertices.push_back({vertex.position, vertex.normal, vertex.uv});
        }

        // Create mesh.
        const std::vector<glm::mat4> &model_matrices =
            entity.mesh->model_matrices.empty() ? std::vector{glm::mat4{1.0f}}
                                                : entity.mesh->model_matrices;

        const auto mesh = std::make_shared<rendering::EntityMesh>(
            m_rendering_context.context, vertices, entity.mesh->indices,
            surfaces, model_matrices, entity.mesh->bounding_box);

        cached_mesh = entity_mesh_cache.insert({entity.mesh, mesh}).first;
      }

      rendering::Entity rendering_entity{
          cached_mesh->second,
          entity_shader,
          entity.model_matrix(),
          entity.wireframe,
      };

      m_rendering_context.entity_tree.add(rendering_entity);
    }

    // Load geodata.
    const auto geodata_entities =
        geodata_loader.load_geodata(map_name, map.position, map.bounding_box);

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
          m_rendering_context.context, blocks, surface,
          entity.mesh->bounding_box);

      rendering::Entity rendering_entity{
          mesh,
          geodata_shader,
          entity.model_matrix(),
          false,
      };

      m_rendering_context.entity_tree.add(rendering_entity);
    }
  }
}

auto LoadingSystem::load_texture(const Texture &texture)
    -> std::shared_ptr<rendering::Texture> {

  if (texture.data == nullptr) {
    return nullptr;
  }

  auto format = rendering::Texture::Format::DXT1;

  switch (texture.format) {
  case TEXTURE_RGBA: {
    format = rendering::Texture::Format::RGBA;
    break;
  }
  case TEXTURE_DXT3: {
    format = rendering::Texture::Format::DXT3;
    break;
  }
  case TEXTURE_DXT5: {
    format = rendering::Texture::Format::DXT5;
    break;
  }
  case TEXTURE_DXT1: {
    break;
  }
  }

  return std::make_shared<rendering::Texture>(m_rendering_context.context,
                                              format, texture.width,
                                              texture.height, texture.data);
}

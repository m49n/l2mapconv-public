#include "pch.h"

#include "Renderer.h"

Renderer::Renderer(RenderingContext &rendering_context,
                   const std::filesystem::path &resource_root)
    : m_rendering_context{rendering_context},
      m_shader_loader{m_rendering_context.context, resource_root / "shaders"},
      m_texture_loader{m_rendering_context.context,
                       resource_root / "textures"} {}

auto Renderer::render_map(const Map &map) const -> rendering::SceneGroupId {
  const auto group = m_rendering_context.scene.create_group();
  const auto entity_shader = m_shader_loader.load_entity_shader("entity");

  std::unordered_map<std::shared_ptr<EntityMesh>,
                     std::shared_ptr<rendering::EntityMesh>>
      entity_mesh_cache;

  std::unordered_map<const unsigned char *, std::shared_ptr<rendering::Texture>>
      texture_cache;

  for (const auto &entity : map.entities) {
    // Load mesh if needed
    auto cached_mesh = entity_mesh_cache.find(entity.mesh);

    if (cached_mesh == entity_mesh_cache.end()) {
      std::vector<rendering::MeshSurface> surfaces;

      for (const auto &surface : entity.mesh->surfaces) {
        // Load texture if needed
        auto cached_texture = texture_cache.find(surface.material.texture.data);

        if (cached_texture == texture_cache.end()) {
          const auto texture = load_texture(surface.material.texture);
          cached_texture =
              texture_cache.insert({surface.material.texture.data, texture})
                  .first;
        }

        // Add surface
        surfaces.emplace_back(
            surface.type,
            rendering::Material{surface.material.color, cached_texture->second},
            surface.index_offset, surface.index_count);
      }

      // Add vertices
      std::vector<rendering::Vertex> vertices;

      for (const auto &vertex : entity.mesh->vertices) {
        vertices.push_back({vertex.position, vertex.normal, vertex.uv});
      }

      const auto mesh = std::make_shared<rendering::EntityMesh>( //
          m_rendering_context.context,                           //
          vertices,                                              //
          entity.mesh->indices,                                  //
          surfaces,                                              //
          entity.instance_matrices(),                            //
          entity.mesh->bounding_box                              //
      );

      cached_mesh = entity_mesh_cache.insert({entity.mesh, mesh}).first;
    }

    rendering::Entity rendering_entity{
        cached_mesh->second,
        entity_shader,
        entity.model_matrix(),
        entity.wireframe,
    };

    m_rendering_context.scene.add(group, rendering_entity);
  }

  return group;
}

void Renderer::render_maps(const std::vector<Map> &maps) const {
  for (const auto &map : maps) {
    (void)render_map(map);
  }
}

auto Renderer::render_geodata_group(
    const std::vector<Entity<GeodataMesh>> &geodata_entities) const
    -> rendering::SceneGroupId {
  const auto group = m_rendering_context.scene.create_group();

  const auto geodata_shader = m_shader_loader.load_entity_shader("geodata");
  const auto nswe_texture = m_texture_loader.load_texture("nswe.png");

  for (const auto &entity : geodata_entities) {
    std::vector<rendering::GeodataCell> cells;

    for (const auto &cell : entity.mesh->cells) {
      cells.push_back({
          static_cast<std::int32_t>(cell.x),
          static_cast<std::int32_t>(cell.y),
          static_cast<std::int32_t>(cell.z),
          static_cast<std::uint16_t>(cell.type),
          cell.north,
          cell.south,
          cell.west,
          cell.east,
      });
    }

    rendering::MeshSurface surface{
        entity.mesh->surface.type,
        rendering::Material{entity.mesh->surface.material.color, nswe_texture},
        0,
        0,
    };

    const auto mesh = std::make_shared<rendering::GeodataMesh>( //
        m_rendering_context.context,                            //
        cells,                                                  //
        surface,                                                //
        entity.mesh->bounding_box                               //
    );

    rendering::Entity rendering_entity{
        mesh,
        geodata_shader,
        entity.model_matrix(),
        false,
    };

    m_rendering_context.scene.add(group, rendering_entity);
  }

  return group;
}

void Renderer::render_geodata(
    const std::vector<Entity<GeodataMesh>> &geodata_entities) const {
  (void)render_geodata_group(geodata_entities);
}

void Renderer::remove(std::uint64_t surface_filter) const {
  m_rendering_context.scene.remove(surface_filter);
}

void Renderer::remove_group(rendering::SceneGroupId group) const {
  m_rendering_context.scene.remove_group(group);
}

auto Renderer::load_texture(const Texture &texture) const
    -> std::shared_ptr<rendering::Texture> {

  if (texture.data == nullptr) {
    return nullptr;
  }

  auto format = rendering::Texture::Format::DXT1;

  switch (texture.format) {
  case TEXTURE_RGBA: {
    format = rendering::Texture::Format::RGBA;
  } break;
  case TEXTURE_DXT1: {
    format = rendering::Texture::Format::DXT1;
  } break;
  case TEXTURE_DXT3: {
    format = rendering::Texture::Format::DXT3;
  } break;
  case TEXTURE_DXT5: {
    format = rendering::Texture::Format::DXT5;
  } break;
  }

  return std::make_shared<rendering::Texture>( //
      m_rendering_context.context,             //
      format,                                  //
      texture.width,                           //
      texture.height,                          //
      texture.data                             //
  );
}

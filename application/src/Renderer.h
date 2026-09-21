#pragma once

#include "Entity.h"
#include "Map.h"
#include "RenderingContext.h"

#include <utils/NonCopyable.h>

#include <rendering/ShaderLoader.h>
#include <rendering/Texture.h>
#include <rendering/TextureLoader.h>

#include <memory>
#include <vector>

class Renderer : public utils::NonCopyable {
public:
  explicit Renderer(RenderingContext &rendering_context,
                    const std::filesystem::path &resource_root);

  void render_maps(const std::vector<Map> &maps) const;
  void render_geodata(
      const std::vector<Entity<GeodataMesh>> &geodata_entities) const;

  void remove(std::uint64_t surface_filter) const;

private:
  RenderingContext &m_rendering_context;
  const rendering::ShaderLoader m_shader_loader;
  const rendering::TextureLoader m_texture_loader;

  auto load_texture(const Texture &texture) const
      -> std::shared_ptr<rendering::Texture>;
};

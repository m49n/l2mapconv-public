#include "pch.h"

#include "GeodataLoader.h"

auto GeodataLoader::load_geodata(const std::string &name,
                                 const glm::vec3 &position,
                                 const math::Box &bounding_box) const
    -> std::vector<Entity<GeodataMesh>> {

  const auto *geodata = m_geodata_loader.load_geodata(name);

  if (geodata == nullptr) {
    return {};
  }

  const auto entity_position = glm::vec3{position.x, position.y, 0.0f};

  const auto mesh = std::make_shared<GeodataMesh>();

  mesh->blocks = geodata->blocks;
  mesh->bounding_box = math::Box{bounding_box.min() - entity_position,
                                 bounding_box.max() - entity_position};

  mesh->surface.type = SURFACE_GEODATA;
  mesh->surface.material.color = {0.0f, 1.0f, 1.0f};

  Entity entity{mesh};
  entity.position = entity_position;

  return {std::move(entity)};
}

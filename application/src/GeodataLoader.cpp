#include "pch.h"

#include "GeodataLoader.h"

auto GeodataLoader::load_geodata(const std::string &name,
                                 const glm::vec3 &position) const
    -> std::vector<Entity<GeodataMesh>> {

  const auto *geodata = m_geodata_loader.load_geodata(name);

  if (geodata == nullptr) {
    return {};
  }

  const auto mesh = std::make_shared<GeodataMesh>();

  mesh->blocks = geodata->blocks;

  mesh->surface.type = SURFACE_GEODATA;
  mesh->surface.material.color = {0.0f, 1.0f, 1.0f};

  Entity entity{mesh};
  entity.position = position;
  entity.position.z = 0.0f;

  return {std::move(entity)};
}

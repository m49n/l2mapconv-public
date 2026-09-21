#include "ImportedGeodataLoader.h"

ImportedGeodataLoader::ImportedGeodataLoader(
    const std::filesystem::path &root_path)
    : m_loader{root_path} {}

auto ImportedGeodataLoader::load(
    const std::string &map_name,
    const geometry::Box &map_bounding_box) const
    -> std::vector<Entity<GeodataMesh>> {
  const auto *geodata = m_loader.load_geodata(map_name);
  if (geodata == nullptr) {
    return {};
  }

  auto mesh = std::make_shared<GeodataMesh>();
  mesh->cells = geodata->cells;
  mesh->bounding_box =
      geometry::Box{{0.0f, 0.0f, map_bounding_box.min().z},
                    map_bounding_box.max() - map_bounding_box.min()};
  mesh->surface.type = SURFACE_IMPORTED_GEODATA;
  mesh->surface.material.color = {0.0f, 1.0f, 1.0f};

  Entity entity{std::move(mesh)};
  entity.position =
      {map_bounding_box.min().x, map_bounding_box.min().y, 0.0f};
  return {std::move(entity)};
}

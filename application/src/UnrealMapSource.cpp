#include "UnrealMapSource.h"

UnrealMapSource::UnrealMapSource(const std::filesystem::path &client_root)
    : m_loader{client_root} {}

auto UnrealMapSource::load(const MapRegion &region, MapLayer layer) -> Map {
  const auto options = layer == MapLayer::Terrain
                           ? MapLoadOptions::terrain_only()
                           : MapLoadOptions::detail_only();
  auto map = m_loader.load_map(region.name, options);
  map.name = region.name;
  return map;
}

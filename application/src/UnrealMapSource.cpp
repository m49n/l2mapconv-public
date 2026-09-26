#include "UnrealMapSource.h"

UnrealMapSource::UnrealMapSource(const std::filesystem::path &client_root)
    : m_loader{client_root}, m_visual_loader{client_root} {}

auto UnrealMapSource::load(const MapRegion &region, MapLayer layer,
                           const territory::Cancel &cancel) -> MapLoadPayload {
  territory::check_cancel(cancel);
  if (layer == MapLayer::Detail) {
    auto visual = std::make_shared<territory::VisualScene>(
        m_visual_loader.load(region.name, cancel));
    territory::check_cancel(cancel);
    // VisualScene owns the textured terrain, mesh actors and BSP. Keep the
    // legacy volume-only path because blocking brushes are a separate overlay
    // and the existing UI switch must still control them.
    auto map = m_loader.load_map(region.name, MapLoadOptions::blocking_only());
    territory::check_cancel(cancel);
    map.name = region.name;
    const auto &bounds = visual->bounds;
    map.bounding_box = geometry::Box{
        {static_cast<float>(bounds.min_x), static_cast<float>(bounds.min_y),
         static_cast<float>(bounds.min_z)},
        {static_cast<float>(bounds.max_x), static_cast<float>(bounds.max_y),
         static_cast<float>(bounds.max_z)}};
    map.position = map.bounding_box.min();
    return {std::move(map), std::move(visual)};
  }
  auto map = m_loader.load_map(region.name, MapLoadOptions::terrain_only());
  map.name = region.name;
  territory::check_cancel(cancel);
  return {std::move(map), {}};
}

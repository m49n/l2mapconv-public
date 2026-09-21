#include "RendererMapSceneSink.h"

RendererMapSceneSink::RendererMapSceneSink(
    Renderer &renderer, RenderingContext &rendering_context,
    const std::filesystem::path &geodata_root)
    : m_renderer{renderer}, m_rendering_context{rendering_context},
      m_imported_geodata{geodata_root} {}

auto RendererMapSceneSink::upload(MapCoordinate /*coordinate*/, MapLayer layer,
                                  const Map &map) -> rendering::SceneGroupId {
  const auto map_group = m_renderer.render_map(map);
  if (layer == MapLayer::Detail) {
    const auto imported = m_imported_geodata.load(map.name, map.bounding_box);
    if (!imported.empty()) {
      const auto geodata_group = m_renderer.render_geodata_group(imported);
      m_imported_groups.insert_or_assign(map_group, geodata_group);
    }
  }
  return map_group;
}

void RendererMapSceneSink::remove(rendering::SceneGroupId group) {
  const auto imported = m_imported_groups.find(group);
  if (imported != m_imported_groups.end()) {
    m_renderer.remove_group(imported->second);
    m_imported_groups.erase(imported);
  }
  m_renderer.remove_group(group);
}

auto RendererMapSceneSink::camera_xy() const -> glm::vec2 {
  const auto position = m_rendering_context.camera.position();
  return {position.x, position.y};
}

void RendererMapSceneSink::place_camera_for_seed(const Map &map) {
  const auto minimum = map.bounding_box.min();
  const auto maximum = map.bounding_box.max();
  m_rendering_context.camera.set_position({(minimum.x + maximum.x) * 0.5f,
                                           (minimum.y + maximum.y) * 0.5f,
                                           maximum.z + 8192.0f});
}

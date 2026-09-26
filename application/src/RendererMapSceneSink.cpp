#include "RendererMapSceneSink.h"

RendererMapSceneSink::RendererMapSceneSink(
    Renderer &renderer, LiveVisualRenderer &live_renderer,
    RenderingContext &rendering_context,
    const std::filesystem::path &geodata_root)
    : m_renderer{renderer}, m_live_renderer{live_renderer},
      m_rendering_context{rendering_context},
      m_imported_geodata{geodata_root} {}

auto RendererMapSceneSink::upload(MapCoordinate /*coordinate*/, MapLayer layer,
                                  const MapLoadPayload &payload)
    -> rendering::SceneGroupId {
  const auto map_group = m_renderer.render_map(payload.map);
  try {
    if (layer == MapLayer::Detail) {
      if (!payload.visual) {
        throw std::runtime_error{"Detail map has no visual scene"};
      }
      m_live_renderer.upload(map_group, *payload.visual);
      const auto imported = m_imported_geodata.load(payload.map.name,
                                                     payload.map.bounding_box);
      if (!imported.empty()) {
        const auto geodata_group = m_renderer.render_geodata_group(imported);
        m_imported_groups.insert_or_assign(map_group, geodata_group);
      }
    }
  } catch (...) {
    remove(map_group);
    throw;
  }
  return map_group;
}

void RendererMapSceneSink::set_visible(rendering::SceneGroupId group,
                                       bool visible) {
  m_rendering_context.scene.set_group_visible(group, visible);
}

void RendererMapSceneSink::remove(rendering::SceneGroupId group) {
  m_live_renderer.remove(group);
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

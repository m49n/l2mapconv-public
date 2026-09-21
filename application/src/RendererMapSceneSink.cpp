#include "RendererMapSceneSink.h"

RendererMapSceneSink::RendererMapSceneSink(Renderer &renderer,
                                           RenderingContext &rendering_context)
    : m_renderer{renderer}, m_rendering_context{rendering_context} {}

auto RendererMapSceneSink::upload(MapCoordinate /*coordinate*/,
                                  MapLayer /*layer*/, const Map &map)
    -> rendering::SceneGroupId {
  return m_renderer.render_map(map);
}

void RendererMapSceneSink::remove(rendering::SceneGroupId group) {
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

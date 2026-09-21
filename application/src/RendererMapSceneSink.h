#pragma once

#include "MapSceneSink.h"
#include "Renderer.h"
#include "RenderingContext.h"

class RendererMapSceneSink final : public MapSceneSink {
public:
  RendererMapSceneSink(Renderer &renderer, RenderingContext &rendering_context);

  auto upload(MapCoordinate coordinate, MapLayer layer, const Map &map)
      -> rendering::SceneGroupId override;
  void remove(rendering::SceneGroupId group) override;
  auto camera_xy() const -> glm::vec2 override;
  void place_camera_for_seed(const Map &map) override;

private:
  Renderer &m_renderer;
  RenderingContext &m_rendering_context;
};

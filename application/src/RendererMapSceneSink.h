#pragma once

#include "ImportedGeodataLoader.h"
#include "MapSceneSink.h"
#include "LiveVisualRenderer.h"
#include "Renderer.h"
#include "RenderingContext.h"

#include <filesystem>
#include <map>

class RendererMapSceneSink final : public MapSceneSink {
public:
  RendererMapSceneSink(Renderer &renderer, LiveVisualRenderer &live_renderer,
                       RenderingContext &rendering_context,
                       const std::filesystem::path &geodata_root = "geodata");

  auto upload(MapCoordinate coordinate, MapLayer layer,
              const MapLoadPayload &payload)
      -> rendering::SceneGroupId override;
  void remove(rendering::SceneGroupId group) override;
  void set_visible(rendering::SceneGroupId group, bool visible) override;
  auto camera_xy() const -> glm::vec2 override;
  void place_camera_for_seed(const Map &map) override;

private:
  Renderer &m_renderer;
  LiveVisualRenderer &m_live_renderer;
  RenderingContext &m_rendering_context;
  ImportedGeodataLoader m_imported_geodata;
  std::map<rendering::SceneGroupId, rendering::SceneGroupId> m_imported_groups;
};

#pragma once

#include "Map.h"
#include "MapCatalog.h"
#include "MapLoadOptions.h"

#include <rendering/Scene.h>

#include <glm/glm.hpp>

class MapSceneSink {
public:
  virtual ~MapSceneSink() = default;
  virtual auto upload(MapCoordinate coordinate, MapLayer layer, const Map &map)
      -> rendering::SceneGroupId = 0;
  virtual void remove(rendering::SceneGroupId group) = 0;
  virtual auto camera_xy() const -> glm::vec2 = 0;
  virtual void place_camera_for_seed(const Map &map) = 0;
};

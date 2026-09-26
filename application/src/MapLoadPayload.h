#pragma once

#include "Map.h"

#include <territory/VisualScene.h>

#include <memory>

struct MapLoadPayload {
  Map map;
  std::shared_ptr<territory::VisualScene> visual;
};

#pragma once

#include "Map.h"
#include "MapCatalog.h"
#include "MapLoadOptions.h"

class MapSource {
public:
  virtual ~MapSource() = default;
  virtual auto load(const MapRegion &region, MapLayer layer) -> Map = 0;
};

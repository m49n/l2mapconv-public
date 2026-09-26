#pragma once

#include "MapLoadPayload.h"
#include "MapCatalog.h"
#include "MapLoadOptions.h"
#include <territory/Job.h>

class MapSource {
public:
  virtual ~MapSource() = default;
  virtual auto load(const MapRegion &region, MapLayer layer,
                    const territory::Cancel &cancel) -> MapLoadPayload = 0;
};

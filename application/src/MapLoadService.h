#pragma once

#include "MapLoadQueue.h"

#include <vector>

class MapLoadService {
public:
  virtual ~MapLoadService() = default;
  virtual void reconcile(std::vector<MapLoadRequest> requests,
                         std::vector<MapLoadKey> cancellations) = 0;
  virtual auto take_started() -> std::vector<MapLoadRequest> = 0;
  virtual auto take_completed() -> std::vector<MapLoadResult> = 0;
  virtual auto is_current(const MapLoadRequest &request) const -> bool = 0;
};

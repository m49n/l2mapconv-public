#include "MapResidency.h"

auto desired_map_residency(const MapCatalog &catalog,
                           const MapResidencyIntent &intent)
    -> DesiredMapResidency {
  DesiredMapResidency desired{.terrain = intent.manual, .detail = {}};
  if (!intent.current || !catalog.contains(*intent.current)) {
    return desired;
  }

  if (!intent.auto_load) {
    // Manual mode shows the complete selected square under the camera;
    // whole-world selections keep only this one Detail scene resident.
    if (intent.manual.contains(*intent.current)) {
      desired.detail.insert(*intent.current);
    }
    return desired;
  }

  if (intent.include_neighbors) {
    const auto neighbors = catalog.neighbors(*intent.current, 1);
    desired.detail.insert(neighbors.begin(), neighbors.end());
  } else {
    desired.detail.insert(*intent.current);
  }
  desired.terrain.insert(desired.detail.begin(), desired.detail.end());
  return desired;
}

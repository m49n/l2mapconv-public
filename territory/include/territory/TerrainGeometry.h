#pragma once
#include "Job.h"
#include "VisualScene.h"
#include <array>
#include <memory>
#include <unreal/Terrain.h>

namespace territory {
struct TerrainGeometry {
  VisualMesh mesh;
  std::size_t clamped_border_samples{};
};
// Order: owner, east, south, southeast. Sample neighbour heights at the owner's
// regular world-space border. Missing coverage is clamped and counted.
// The output crop is independent of each actor's world transform.
TerrainGeometry build_terrain_mesh(
    const std::array<std::shared_ptr<unreal::TerrainInfoActor>, 4> &tiles,
    const Bounds &crop, const Cancel &cancel = {});
} // namespace territory

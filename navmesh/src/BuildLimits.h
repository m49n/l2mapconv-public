#pragma once

#include <span>

namespace navmesh::detail {
// Recast polygon layout: nvp vertex indices followed by nvp neighbors.
// Recast detail layout: vertex base/count and triangle base/count per polygon.
void validate_detail_limits(std::span<const unsigned short> polygons, int nvp,
                            std::span<const unsigned int> meshes);
} // namespace navmesh::detail

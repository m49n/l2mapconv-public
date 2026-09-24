#pragma once
#include "Job.h"
#include "VisualScene.h"

namespace territory {
struct GeometricNormalMesh {
  VisualMesh mesh;
  std::size_t skipped_triangles{};
};
// Select one draw, retain local positions/UVs/colors, and replace smooth
// normals with world-space face normals. The caller must use an identity normal
// matrix.
GeometricNormalMesh rebuild_geometric_normals(const VisualMesh &, const Draw &,
                                              const Cancel &cancel = {});
} // namespace territory

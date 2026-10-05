#pragma once
#include "RegionMetadata.h"
#include <span>
namespace navmesh {
struct RegionFile {
  std::string map;
  std::filesystem::path mesh_path;
};
struct WorldMesh {
  Mesh mesh;
  std::vector<RegionMetadata> regions;
};
WorldMesh load_world(std::span<const RegionFile>, const Cancel & = {});
} // namespace navmesh

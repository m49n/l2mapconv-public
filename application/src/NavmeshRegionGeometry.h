#pragma once
#include <navmesh/RegionMetadata.h>
#include <unreal/ArchiveLoader.h>
struct NavmeshRegionGeometry {
  navmesh::InputGeometry geometry;
  navmesh::RegionMetadata metadata;
};
using CollisionMapLoad = std::function<geodata::Map(
    const std::string &, unreal::ArchiveReadObserver)>;
NavmeshRegionGeometry
load_navmesh_region_geometry(const std::filesystem::path &client,
                             const std::string &map, const navmesh::Settings &,
                             const navmesh::Cancel &, const CollisionMapLoad &);
void verify_navmesh_sources(const std::filesystem::path &,
                            const navmesh::RegionMetadata &,
                            const navmesh::Cancel &);

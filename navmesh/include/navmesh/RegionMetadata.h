#pragma once
#include "Navmesh.h"
#include <territory/FileIdentity.h>
#include <territory/Json.h>

namespace navmesh {
struct SourcePackage {
  std::string relative_path, sha256;
  std::uint64_t size{};
};
struct RegionMetadata {
  std::string map, generator;
  Settings settings{};
  double padding{};
  bool neighbor_context{};
  std::vector<std::string> loaded_neighbors{}, missing_neighbors{};
  std::vector<SourcePackage> sources{};
  territory::FileIdentity mesh{};
};
glm::dvec4 region_bounds(const std::string &map);
std::vector<std::string> region_neighbors(const std::string &map);
territory::Json region_profile(const Settings &);
std::filesystem::path metadata_path(const std::filesystem::path &mesh);
RegionMetadata read_region_metadata(const std::filesystem::path &mesh);
void write_region_metadata_new(const RegionMetadata &,
                               const std::filesystem::path &mesh,
                               const Cancel & = {});
} // namespace navmesh

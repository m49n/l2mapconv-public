#pragma once

#include "Settings.h"
#include "InputGeometry.h"
#include <DetourNavMesh.h>
#include <exception>
#include <filesystem>
#include <functional>
#include <geodata/Map.h>
#include <memory>

namespace navmesh {
struct MeshDeleter {
  void operator()(dtNavMesh *mesh) const { dtFreeNavMesh(mesh); }
};
using Mesh = std::unique_ptr<dtNavMesh, MeshDeleter>;
struct Result {
  Mesh mesh;
  std::size_t tiles{}, polygons{}, empty_tiles{};
};
using Cancel = std::function<bool()>;
using Progress = std::function<void(std::size_t, std::size_t)>;
struct Cancelled : std::exception {
  const char *what() const noexcept override { return "Navmesh cancelled"; }
};
Result build(const geodata::Map &, const Settings &, const Cancel & = {},
             const Progress & = {});
Result build(const InputGeometry &, const Settings &, const Cancel & = {},
             const Progress & = {});
// Public query inputs are game coordinates (Z up), not Detour coordinates.
bool reachable(const dtNavMesh &, glm::vec3 from, glm::vec3 to,
               float horizontal_tolerance = 8, float vertical_tolerance = 32);
void save(const dtNavMesh &, const std::filesystem::path &,
          const Cancel & = {});
Mesh load(const std::filesystem::path &);
} // namespace navmesh

#pragma once
#include "Settings.h"
#include <functional>
#include <geodata/Map.h>

namespace navmesh {
// Output bounds are game XYZ; triangle vertices are Recast XZY.
struct InputGeometry {
  geometry::Box game_bounds;
  std::vector<glm::vec3> vertices{};
  std::vector<unsigned int> indices{};
};
InputGeometry input_geometry(const geodata::Map &,
                             const std::function<bool()> &cancel = {});
double context_padding(const Settings &);
void validate_region_grid(const Settings &);
void append_context(InputGeometry &, const geodata::Map &, double padding,
                    const std::function<bool()> &cancel = {});
} // namespace navmesh

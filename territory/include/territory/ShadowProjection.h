#pragma once
#include "VisualScene.h"
#include <glm/glm.hpp>

namespace territory {
struct ShadowProjection {
  glm::mat4 relative_world_to_clip{1.f};
  glm::vec3 surface_to_sun{};
  int size{};
};

ShadowProjection make_shadow_projection(const Bounds &bounds,
                                        double azimuth_deg,
                                        double elevation_deg, int size);
ShadowProjection make_shadow_projection(const Bounds &output_bounds,
                                        const Bounds &coverage_bounds,
                                        double azimuth_deg,
                                        double elevation_deg, int size);
} // namespace territory

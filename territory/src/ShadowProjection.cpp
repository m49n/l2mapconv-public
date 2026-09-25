#include <territory/ShadowProjection.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <stdexcept>

namespace territory {
ShadowProjection make_shadow_projection(const Bounds &output_bounds,
                                        const Bounds &bounds,
                                        double azimuth_deg,
                                        double elevation_deg, int size) {
  if (size <= 0 || !std::isfinite(azimuth_deg) ||
      !std::isfinite(elevation_deg))
    throw std::invalid_argument("Invalid shadow projection settings");
  const glm::dvec3 extent(bounds.max_x - bounds.min_x,
                          bounds.max_y - bounds.min_y,
                          bounds.max_z - bounds.min_z);
  if (extent.x <= 0 || extent.y <= 0 || extent.z < 0 ||
      !std::isfinite(extent.x) || !std::isfinite(extent.y) ||
      !std::isfinite(extent.z))
    throw std::invalid_argument("Invalid shadow projection bounds");
  const auto azimuth = glm::radians(azimuth_deg);
  const auto elevation = glm::radians(elevation_deg);
  const glm::dvec3 sun(std::sin(azimuth) * std::cos(elevation),
                       -std::cos(azimuth) * std::cos(elevation),
                       std::sin(elevation));
  const double left = bounds.min_x - output_bounds.min_x;
  const double right = bounds.max_x - output_bounds.min_x;
  const double top = bounds.min_y - output_bounds.min_y;
  const double bottom = bounds.max_y - output_bounds.min_y;
  const glm::dvec3 center((left + right) / 2, (top + bottom) / 2,
                          (bounds.min_z + bounds.max_z) / 2);
  const auto distance = glm::length(extent) * 2 + 1;
  const auto view = glm::lookAt(center + sun * distance, center,
                                glm::dvec3(0, 0, 1));
  glm::dvec3 minimum(std::numeric_limits<double>::max());
  glm::dvec3 maximum(-std::numeric_limits<double>::max());
  for (double x : {left, right})
    for (double y : {top, bottom})
      for (double z : {bounds.min_z, bounds.max_z}) {
        const glm::dvec3 p = view * glm::dvec4(x, y, z, 1);
        minimum = glm::min(minimum, p);
        maximum = glm::max(maximum, p);
      }
  const auto pad_x = std::max((maximum.x - minimum.x) * 2 / size, 0.001);
  const auto pad_y = std::max((maximum.y - minimum.y) * 2 / size, 0.001);
  const auto near = std::max(0.001, -maximum.z - 1.0);
  const auto far = -minimum.z + 1.0;
  const auto projection = glm::ortho(minimum.x - pad_x, maximum.x + pad_x,
                                     minimum.y - pad_y, maximum.y + pad_y,
                                     near, far);
  const glm::dmat4 combined = projection * view;
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 4; ++row)
      if (!std::isfinite(combined[column][row]))
        throw std::runtime_error("Nonfinite shadow projection");
  return {glm::mat4(combined), glm::vec3(sun), size};
}
ShadowProjection make_shadow_projection(const Bounds &bounds,
                                        double azimuth_deg,
                                        double elevation_deg, int size) {
  return make_shadow_projection(bounds, bounds, azimuth_deg, elevation_deg,
                                size);
}
} // namespace territory

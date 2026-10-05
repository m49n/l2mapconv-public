#include "PathfindingView.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace {
void finite(glm::dvec2 p) {
  if (!std::isfinite(p.x) || !std::isfinite(p.y))
    throw std::invalid_argument("Nonfinite view coordinates");
}
} // namespace
void validate_top_view(const TopView &v, glm::ivec2 viewport) {
  finite(v.center);
  if (viewport.x <= 0 || viewport.y <= 0 || !std::isfinite(v.width) ||
      v.width < 128 || v.width > max_top_view_width || !std::isfinite(v.z_min) ||
      !std::isfinite(v.z_max) || v.z_min >= v.z_max)
    throw std::invalid_argument("Invalid top view or viewport");
}
auto screen_to_world_xy(const TopView &v, glm::dvec2 p, glm::ivec2 viewport)
    -> glm::dvec2 {
  validate_top_view(v, viewport);
  finite(p);
  return v.center + (p - glm::dvec2(viewport) * 0.5) * (v.width / viewport.x);
}
auto world_to_screen_xy(const TopView &v, glm::dvec2 p, glm::ivec2 viewport)
    -> glm::dvec2 {
  validate_top_view(v, viewport);
  finite(p);
  return glm::dvec2(viewport) * 0.5 + (p - v.center) * (viewport.x / v.width);
}
void zoom_top_view(TopView &v, double wheel, glm::dvec2 pixel,
                   glm::ivec2 viewport) {
  const auto anchor = screen_to_world_xy(v, pixel, viewport);
  if (!std::isfinite(wheel))
    throw std::invalid_argument("Nonfinite wheel motion");
  v.width =
      std::clamp(v.width * std::exp(std::clamp(-wheel * 0.2, -20.0, 20.0)),
                 128.0, max_top_view_width);
  v.center += anchor - screen_to_world_xy(v, pixel, viewport);
}
void pan_top_view(TopView &v, glm::dvec2 delta, glm::ivec2 viewport) {
  validate_top_view(v, viewport);
  finite(delta);
  v.center -= delta * (v.width / viewport.x);
}

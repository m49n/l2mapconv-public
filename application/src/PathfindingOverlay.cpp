#include "PathfindingOverlay.h"
#include <algorithm>
std::vector<RouteSegment> navigation_polygon_lines(
    std::span<const std::vector<pathfinding::WorldPoint>> polygons,
    const TopView &view) {
  std::vector<RouteSegment> out;
  for (const auto &poly : polygons) {
    if (poly.empty())
      continue;
    double low = poly[0].z, high = low;
    for (const auto &p : poly) {
      low = std::min(low, p.z);
      high = std::max(high, p.z);
    }
    if (high < view.z_min || low > view.z_max)
      continue;
    for (std::size_t i = 0; i < poly.size(); ++i)
      out.push_back(
          {poly[i], poly[(i + 1) % poly.size()], 0x9060e0ffu, false, false});
  }
  return out;
}
unsigned navigation_cell_width(const TopView &view, glm::ivec2 viewport) {
  if (viewport.x <= 0 || viewport.y <= 0 || !std::isfinite(view.width) ||
      view.width <= 0)
    throw std::invalid_argument("Invalid navigation display viewport");
  unsigned width = 16;
  while (width < 32768 && width * double(viewport.x) / view.width < 6.)
    width *= 2;
  return width;
}
RouteOverlay route_overlay(const PathfindingContext &c,
                           std::span<const pathfinding::RouteResult> results) {
  RouteOverlay out;
  const auto add = [&](const std::vector<pathfinding::WorldPoint> &points,
                       unsigned color, bool raw) {
    for (std::size_t i = 1; i < points.size(); ++i) {
      auto a = points[i - 1], b = points[i];
      const bool outside = a.z < c.view.z_min || a.z > c.view.z_max ||
                           b.z < c.view.z_min || b.z > c.view.z_max;
      out.outside += outside;
      if (outside && !c.show_full_route) {
        if (std::max(a.z, b.z) < c.view.z_min ||
            std::min(a.z, b.z) > c.view.z_max)
          continue;
        const auto at = [&](double z) {
          const auto t = (z - a.z) / (b.z - a.z);
          return pathfinding::WorldPoint{a.x + (b.x - a.x) * t,
                                         a.y + (b.y - a.y) * t, z};
        };
        const auto old = a;
        if (a.z < c.view.z_min)
          a = at(c.view.z_min);
        else if (a.z > c.view.z_max)
          a = at(c.view.z_max);
        // Interpolate B against the original segment, not the already clipped
        // A.
        const auto end = b;
        if (b.z < c.view.z_min || b.z > c.view.z_max) {
          const auto z = std::clamp(b.z, c.view.z_min, c.view.z_max),
                     t = (z - old.z) / (end.z - old.z);
          b = {old.x + (end.x - old.x) * t, old.y + (end.y - old.y) * t, z};
        }
      }
      out.segments.push_back({a, b, color, raw, outside});
    }
  };
  for (const auto &r : results) {
    const unsigned color =
        c.revision != c.submitted_revision
            ? 0xff909090u
            : (r.backend == "l2j" ? 0xff50b4ffu : 0xffffdc50u);
    if (c.show_raw)
      add(r.raw, color, true);
    if (c.show_final)
      add(r.final_path, color, false);
    if (c.show_validated)
      add(r.validated_samples,
          c.revision != c.submitted_revision
              ? 0xff909090u
              : (r.segments_valid.value_or(false) ? 0xff60ff60u : 0xff5050ffu),
          true);
  }
  return out;
}

#pragma once
#include "PathfindingContext.h"
#include <span>
struct RouteSegment {
  pathfinding::WorldPoint a, b;
  unsigned color{};
  bool raw{}, outside{};
};
struct RouteOverlay {
  std::vector<RouteSegment> segments;
  std::size_t outside{};
};
RouteOverlay route_overlay(const PathfindingContext &,
                           std::span<const pathfinding::RouteResult>);
std::vector<RouteSegment>
navigation_polygon_lines(std::span<const std::vector<pathfinding::WorldPoint>>,
                         const TopView &);
unsigned navigation_cell_width(const TopView &, glm::ivec2 viewport);

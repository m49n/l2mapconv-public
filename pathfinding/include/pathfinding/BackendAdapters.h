#pragma once
#include "Result.h"
#include <string_view>
namespace pathfinding {
std::vector<std::string> visited_navigation_regions(const std::vector<WorldPoint>&,
                                                   const std::vector<std::string>&);
std::string legacy_case_csv(const RouteCase &, int geo_first_x,
                            int geo_first_y);
RouteResult parse_legacy_jsonl(std::string_view, const RouteCase &,
                               std::string request_id);
RouteResult parse_navmesh_json(const Json &, const RouteCase &,
                               std::string request_id);
} // namespace pathfinding

#pragma once
#include "PathfindingContext.h"
#include "PathfindingController.h"
#include "PathfindingLoader.h"
#include "PathfindingOverlay.h"
#include "PathfindingResults.h"
#include <array>
struct PathfindingWindow {
  PathfindingContext context;
  PathfindingLoader loader;
  std::filesystem::path l2j, navmesh, profile_path;
  std::vector<std::filesystem::path> nav_regions;
  pathfinding::BackendProfile profile;
  pathfinding::SearchSettings search;
  pathfinding::BenchmarkSettings benchmark;
  std::array<char, 16> map{};
  std::string case_id;
  std::optional<pathfinding::RouteCase> loaded_case;
  std::map<std::string, std::string> expected_backends;
  std::vector<std::vector<pathfinding::WorldPoint>> polygons;
  std::string message;
  PathfindingResultsState results_state;
  std::shared_ptr<const pathfinding::Dataset> overlay_dataset;
  TopView overlay_view;
  glm::ivec2 overlay_size{};
  std::vector<RouteSegment> nav_lines;
  std::vector<pathfinding::OverviewCell> geo_cells;
  bool cached_cells{}, cached_polygons{};
  unsigned cell_width{};
  void frame(PathfindingController &, rendering::Camera &,
             std::string_view current_map = {});
  void update_navigation_overlay(glm::ivec2);
  void load();
  pathfinding::RouteCase route() const;
};

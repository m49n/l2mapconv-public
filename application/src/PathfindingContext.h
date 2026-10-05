#pragma once
#include "PathfindingView.h"
#include <memory>
#include <optional>
#include <pathfinding/Dataset.h>
#include <rendering/Camera.h>

enum class PickMode { None, A, B };
struct PathfindingInput {
  glm::dvec2 pixel{}, delta{};
  glm::ivec2 viewport{};
  bool pressed{}, released{}, left{}, right{}, middle{}, captured{}, escape{};
  double wheel{};
};
struct PathfindingContext {
  std::shared_ptr<const pathfinding::Dataset> dataset;
  std::optional<pathfinding::Endpoint> a, b;
  TopView view;
  std::optional<rendering::CameraState> flight;
  PickMode picking{PickMode::None};
  std::vector<pathfinding::ResolvedEndpoint> candidates;
  std::vector<std::map<std::string, double>> candidate_heights;
  std::optional<glm::dvec2> press;
  std::optional<glm::dvec2> hover;
  bool show_final{true}, show_validated{};
  bool dragged{}, show_raw{}, show_cells{}, show_polygons{}, show_full_route{};
  std::uint64_t revision{}, submitted_revision{};
  std::string error;
  void choose_floor(std::size_t);
  void offer_candidates(std::vector<pathfinding::ResolvedEndpoint>);
  void swap_points();
  void clear_points();
  void toggle_top(rendering::Camera &);
  void focus(glm::vec3 target, rendering::Camera &);
  void select_profile(const std::filesystem::path &candidate,
                      pathfinding::BackendProfile &,
                      std::filesystem::path &selected);
};
void update_pathfinding_input(PathfindingContext &, const PathfindingInput &);
std::string default_navigation_map(std::string_view selected,
                                   std::string_view current, bool has_inputs);

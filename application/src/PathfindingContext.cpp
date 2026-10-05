#include "PathfindingContext.h"
std::string default_navigation_map(std::string_view selected,
                                   std::string_view current, bool has_inputs) {
  if (selected.empty() && !has_inputs && !current.empty()) {
    try {
      (void)pathfinding::region_origin(std::string(current));
      return std::string(current);
    } catch (const std::invalid_argument &) {
    }
  }
  return std::string(selected);
}
#include <algorithm>
#include <territory/PathIO.h>
void PathfindingContext::select_profile(const std::filesystem::path &candidate,
                                        pathfinding::BackendProfile &profile,
                                        std::filesystem::path &selected) {
  auto parsed = pathfinding::read_profile(candidate);
  auto path = candidate;
  profile = std::move(parsed);
  selected = std::move(path);
  ++revision;
}
void PathfindingContext::choose_floor(std::size_t index) {
  if (index >= candidates.size() || picking == PickMode::None)
    return;
  const auto p = candidates[index].resolved;
  pathfinding::Endpoint point{p, p.z};
  if (index < candidate_heights.size())
    point.backend_z = candidate_heights[index];
  if (picking == PickMode::A) {
    a = point;
    picking = PickMode::B;
  } else {
    b = point;
    picking = PickMode::None;
  }
  candidates.clear();
  ++revision;
}
void PathfindingContext::offer_candidates(
    std::vector<pathfinding::ResolvedEndpoint> values) {
  const auto backend = [](const auto &v) {
    return v.surface_id.starts_with("l2j:") ? std::string("l2j")
                                            : std::string("navmesh");
  };
  // Adjacent nav polygons can describe the same surface at a boundary.
  std::vector<pathfinding::ResolvedEndpoint> unique;
  for (auto &v : values)
    if (std::none_of(unique.begin(), unique.end(), [&](const auto &other) {
          return backend(v) == backend(other) && v.valid == other.valid &&
                 std::abs(v.resolved.z - other.resolved.z) < .01;
        }))
      unique.push_back(std::move(v));
  candidates.clear();
  candidate_heights.clear();
  std::vector<bool> used(unique.size());
  const auto matches = [&](std::size_t i) {
    std::vector<std::size_t> result;
    for (std::size_t j = 0; j < unique.size(); ++j)
      if (backend(unique[i]) != backend(unique[j]) &&
          std::abs(unique[i].resolved.z - unique[j].resolved.z) <= 32)
        result.push_back(j);
    return result;
  };
  for (std::size_t i = 0; i < unique.size(); ++i) {
    if (used[i])
      continue;
    used[i] = true;
    auto representative = unique[i];
    std::map<std::string, double> heights{
        {backend(unique[i]), unique[i].resolved.z}};
    const auto other = matches(i);
    if (other.size() == 1 && !used[other[0]] && matches(other[0]).size() == 1) {
      const auto j = other[0];
      used[j] = true;
      heights[backend(unique[j])] = unique[j].resolved.z;
      representative.surface_id = "L2J + Navmesh";
      representative.valid = unique[i].valid && unique[j].valid;
    }
    candidates.push_back(std::move(representative));
    candidate_heights.push_back(std::move(heights));
  }
  if (candidates.empty())
    error = "No navigation surface at this XY; previous point retained.";
  else {
    error.clear();
    if (candidates.size() == 1)
      choose_floor(0);
  }
}
void PathfindingContext::swap_points() {
  std::swap(a, b);
  candidates.clear();
  ++revision;
}
void PathfindingContext::clear_points() {
  a.reset();
  b.reset();
  candidates.clear();
  press.reset();
  picking = PickMode::None;
  ++revision;
}
void PathfindingContext::toggle_top(rendering::Camera &camera) {
  press.reset();
  candidates.clear();
  picking = PickMode::None;
  if (flight) {
    camera.restore(*flight);
    flight.reset();
  } else {
    flight = camera.snapshot();
    view.center = a ? glm::dvec2{a->requested.x, a->requested.y}
                    : glm::dvec2(camera.position());
    camera.set_top_view(view.center, float(view.width), float(view.z_min),
                        float(view.z_max));
  }
}
void PathfindingContext::focus(glm::vec3 target, rendering::Camera &camera) {
  if (flight) {
    view.center = glm::dvec2(target);
    camera.set_top_view(view.center, float(view.width), float(view.z_min),
                        float(view.z_max));
  } else
    camera.set_position(target);
}
void update_pathfinding_input(PathfindingContext &c,
                              const PathfindingInput &i) {
  c.hover.reset();
  if (i.escape) {
    c.press.reset();
    c.candidates.clear();
    c.picking = PickMode::None;
    return;
  }
  if (!c.flight || i.viewport.x <= 0 || i.viewport.y <= 0) {
    c.press.reset();
    return;
  }
  if (i.captured || i.right || i.middle) {
    c.press.reset();
  }
  if (!i.captured && !i.right) {
    if (i.middle)
      pan_top_view(c.view, i.delta, i.viewport);
    if (i.wheel)
      zoom_top_view(c.view, i.wheel, i.pixel, i.viewport);
  }
  if (i.captured || i.right || i.middle)
    return;
  if (c.picking != PickMode::None)
    c.hover = screen_to_world_xy(c.view, i.pixel, i.viewport);
  if (i.pressed) {
    c.press = i.pixel;
    c.dragged = false;
  }
  if (c.press && glm::length(i.pixel - *c.press) > 4)
    c.dragged = true;
  if (i.released) {
    if (c.press && !c.dragged && c.picking != PickMode::None && c.dataset &&
        c.candidates.empty()) {
      const auto xy = screen_to_world_xy(c.view, i.pixel, i.viewport);
      auto values = c.dataset->candidates(xy.x, xy.y);
      std::erase_if(values, [&](const auto &v) {
        return v.resolved.z < c.view.z_min || v.resolved.z > c.view.z_max;
      });
      c.offer_candidates(std::move(values));
    }
    c.press.reset();
  }
}

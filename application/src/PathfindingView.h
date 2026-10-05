#pragma once
#include <glm/glm.hpp>

// The complete supported 00_00..99_99 grid, plus wide-viewport framing margin.
inline constexpr double max_top_view_width = 16777216.0;

struct TopView {
  glm::dvec2 center{};
  double width{4096}, z_min{-32768}, z_max{32768};
};
void validate_top_view(const TopView &, glm::ivec2 viewport);
auto screen_to_world_xy(const TopView &, glm::dvec2 pixel, glm::ivec2 viewport)
    -> glm::dvec2;
auto world_to_screen_xy(const TopView &, glm::dvec2 world, glm::ivec2 viewport)
    -> glm::dvec2;
void zoom_top_view(TopView &, double wheel, glm::dvec2 pixel,
                   glm::ivec2 viewport);
void pan_top_view(TopView &, glm::dvec2 pixel_delta, glm::ivec2 viewport);

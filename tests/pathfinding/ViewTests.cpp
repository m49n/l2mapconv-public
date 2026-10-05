#include "PathfindingView.h"
#include "Support.h"
#include <limits>
using namespace pf_test;
int view_tests() {
  TopView v{{-212992, 212992}, 4096, -5000, 5000};
  int n = 0;
  n += check("multi-region framing supports picking and zoom above one square", [&] {
    TopView world{{98304,131072}, 100000, -32768,32768};
    const glm::ivec2 viewport{1440,1000};
    const glm::dvec2 point{81962,147434};
    const auto pixel=world_to_screen_xy(world,point,viewport);
    if(glm::length(screen_to_world_xy(world,pixel,viewport)-point)>1e-8)return false;
    zoom_top_view(world,-1,pixel,viewport);
    return world.width>100000 && glm::length(screen_to_world_xy(world,pixel,viewport)-point)<1e-8;
  });
  n += check("center maps to world center at both DPI scales", [&] {
    return screen_to_world_xy(v, {400, 300}, {800, 600}) == v.center &&
           screen_to_world_xy(v, {800, 600}, {1600, 1200}) == v.center;
  });
  n += check(
      "north is negative game Y and corner roundtrip survives resize", [&] {
        const auto p = screen_to_world_xy(v, {0, 0}, {800, 600});
        return p == glm::dvec2{-215040, 211456} &&
               world_to_screen_xy(v, p, {1600, 1200}) == glm::dvec2{0, 0};
      });
  n += check("zero viewport and nonfinite or out-of-range zoom rejected", [&] {
    auto bad = v;
    bad.width = 0;
    auto nan = v;
    nan.center.x = std::numeric_limits<double>::quiet_NaN();
    return rejects([&] { validate_top_view(v, {0, 600}); }) &&
           rejects([&] { validate_top_view(bad, {800, 600}); }) &&
           rejects([&] { validate_top_view(nan, {800, 600}); });
  });
  n += check("zoom anchors cursor world point and clamps extremes", [&] {
    auto zoomed = v;
    zoom_top_view(zoomed, 1, {100, 200}, {800, 600});
    if (zoomed.width >= v.width ||
        glm::length(screen_to_world_xy(zoomed, {100, 200}, {800, 600}) -
                    screen_to_world_xy(v, {100, 200}, {800, 600})) > 1e-8)
      return false;
    zoom_top_view(zoomed, 1e6, {400, 300}, {800, 600});
    if (zoomed.width != 128)
      return false;
    zoom_top_view(zoomed, -1e6, {400, 300}, {800, 600});
    const auto ceiling=zoomed.width;
    zoom_top_view(zoomed,-1e6,{400,300},{800,600});
    return ceiling>100000 && zoomed.width==ceiling && std::isfinite(ceiling);
  });
  n += check("pan uses logical pixel dimensions not framebuffer scale", [&] {
    auto moved = v;
    pan_top_view(moved, {100, -100}, {800, 600});
    return moved.center == glm::dvec2{-213504, 213504};
  });
  return n;
}

#include "PathfindingContext.h"
#include "Support.h"
using namespace pf_test;
using namespace pathfinding;
int input_tests() {
  int n = 0;
  n += check("empty navigation map defaults to current camera region without "
             "switching loaded inputs",
             [] {
               return default_navigation_map("", "22_22", false) == "22_22" &&
                      default_navigation_map("25_19", "22_22", false) ==
                          "25_19" &&
                      default_navigation_map("", "22_22", true).empty() &&
                      default_navigation_map("", "outside", false).empty();
             });
  n += check("malformed replacement profile leaves selected backend and "
             "revision unchanged",
             [] {
               TempDirectory temp;
               auto old = temp.file("old-profile.json", "{}");
               const auto bad = temp.file("bad-profile.json", "{broken");
               PathfindingContext c;
               c.revision = 4;
               pathfinding::BackendProfile p;
               p.java = temp.path / "java.exe";
               auto selected = old;
               return rejects([&] { c.select_profile(bad, p, selected); }) &&
                      selected == old && p.java == temp.path / "java.exe" &&
                      c.revision == 4;
             });
  const ResolvedEndpoint low{{10, 20, 0}, {10, 20, 0}, true, "l2j:low"};
  const ResolvedEndpoint high{{10, 20, 128}, {10, 20, 128}, true, "nav:high"};
  n += check(
      "same floor from two formats is one click with backend-specific heights",
      [&] {
        PathfindingContext c;
        c.picking = PickMode::A;
        c.offer_candidates(
            {low, {{10, 20, -23}, {10, 20, -23}, true, "nav:low"}});
        if (!c.a || c.picking != PickMode::B || !c.candidates.empty())
          return false;
        return c.a->backend_z.at("l2j") == 0 &&
               c.a->backend_z.at("navmesh") == -23;
      });
  n += check("nearby genuine floors are not merged through a shared "
             "other-format candidate",
             [&] {
               PathfindingContext c;
               c.picking = PickMode::A;
               c.offer_candidates(
                   {low,
                    {{10, 20, 24}, {10, 20, 24}, true, "l2j:upper"},
                    {{10, 20, 12}, {10, 20, 12}, true, "nav:ambiguous"}});
               return !c.a && c.candidates.size() == 3;
             });
  n += check("ambiguous floor requires explicit choice, then A arms B", [&] {
    PathfindingContext c;
    c.picking = PickMode::A;
    c.offer_candidates({low, high});
    if (c.a || c.candidates.size() != 2)
      return false;
    c.choose_floor(1);
    return c.a && c.a->confirmed_z == 128 && c.picking == PickMode::B &&
           c.candidates.empty();
  });
  n += check("empty column preserves A, clear retains dataset and swap changes "
             "revision",
             [&] {
               PathfindingContext c;
               c.picking = PickMode::A;
               c.offer_candidates({low});
               c.offer_candidates({});
               if (!c.a || c.b)
                 return false;
               c.offer_candidates({high});
               c.swap_points();
               if (!c.a || !c.b || c.a->confirmed_z != 128 ||
                   c.b->confirmed_z != 0 || c.revision != 3)
                 return false;
               c.clear_points();
               return !c.a && !c.b && c.revision == 4;
             });
  n += check(
      "Escape cancels pending choice without clearing confirmed endpoints",
      [&] {
        PathfindingContext c;
        c.picking = PickMode::A;
        c.offer_candidates({low});
        c.offer_candidates({low, high});
        PathfindingInput i;
        i.escape = true;
        update_pathfinding_input(c, i);
        return c.a && c.candidates.empty() && c.picking == PickMode::None;
      });
  n += check("top projection is north-up and restores exact flight state", [] {
    rendering::Context gpu{};
    gpu.framebuffer.size = {800, 600};
    rendering::Camera camera(gpu, 60, .1, {11, 22, 33});
    camera.rotate(.7, {0, 0, 1});
    auto old = camera.snapshot();
    PathfindingContext c;
    c.toggle_top(camera);
    c.view = {{100, 200}, 800, -10, 90};
    camera.set_top_view(c.view.center, 800, -10, 90);
    const auto m = camera.projection_matrix() * camera.view_matrix();
    const auto p = m * glm::vec4{100, 200, 90, 1},
               q = m * glm::vec4{500, -100, -10, 1};
    if (!camera.top_view() ||
        glm::length(glm::vec3(p) - glm::vec3{0, 0, -1}) > .001f ||
        glm::length(glm::vec3(q) - glm::vec3{1, 1, 1}) > .001f)
      return false;
    c.toggle_top(camera);
    auto restored = camera.snapshot();
    return !camera.top_view() && old.position == restored.position &&
           old.orientation == restored.orientation;
  });
  n += check("Go in top mode preserves width and endpoints", [&] {
    rendering::Context gpu{};
    gpu.framebuffer.size = {800, 600};
    rendering::Camera camera(gpu, 60, .1, {0, 0, 0});
    PathfindingContext c;
    c.toggle_top(camera);
    c.view.width = 1234;
    c.a = Endpoint{low.resolved, 0};
    c.focus({-212992, 212992, 32768}, camera);
    return c.view.center == glm::dvec2{-212992, 212992} &&
           c.view.width == 1234 && c.a && c.a->requested == low.resolved;
  });
  n += check("entering top view focuses an already loaded case without losing "
             "the flight snapshot",
             [] {
               rendering::Context gpu{};
               gpu.framebuffer.size = {800, 600};
               rendering::Camera camera(gpu, 60, .1, {10, 20, 30});
               PathfindingContext c;
               c.a = Endpoint{{166136, 53528, -4112}, -4112};
               c.toggle_top(camera);
               return c.view.center == glm::dvec2{166136, 53528} &&
                      c.flight->position == glm::vec3{10, 20, 30};
             });
  n += check("UI capture, drag and RMB cannot leave an armed click", [] {
    PathfindingContext c;
    c.flight = rendering::CameraState{};
    c.picking = PickMode::A;
    PathfindingInput i;
    i.viewport = {800, 600};
    i.pressed = true;
    i.pixel = {100, 100};
    i.captured = true;
    update_pathfinding_input(c, i);
    if (c.press)
      return false;
    i.captured = false;
    update_pathfinding_input(c, i);
    if (!c.press)
      return false;
    i.pressed = false;
    i.left = true;
    i.pixel = {106, 100};
    update_pathfinding_input(c, i);
    if (!c.dragged)
      return false;
    i.released = true;
    i.right = true;
    update_pathfinding_input(c, i);
    return !c.press && !c.a;
  });
  return n;
}

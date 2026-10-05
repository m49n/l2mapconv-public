#include "PathfindingController.h"
#include "Support.h"
#include <thread>
using namespace pf_test;
int controller_tests(const std::filesystem::path &exe,
                     const std::filesystem::path &child) {
  TempDirectory input, output;
  pathfinding::RouteCase route{
      "controller",
      "25_19",
      {{166136, 53528, -4112}, -4112},
      {{166136, 53544, -4112}, -4112},
      pathfinding::identify_file(input.file("25_19.l2j", "abc")),
      pathfinding::identify_file(input.file("25_19.navmesh", "nav"))};
  const auto finish = [](PathfindingController &c) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    do {
      c.poll();
      if (!c.active())
        return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    c.cancel();
    return false;
  };
  int n = 0;
  n += check("controller keeps viewer usable and retains two missing-backend "
             "diagnostics",
             [&] {
               PathfindingController c(exe, output.path / "normal");
               if (!c.start(route, {}))
                 return false;
               const auto id = c.request_id();
               if (c.start(route, {}) || c.request_id() != id)
                 return false;
               if (!finish(c) || c.results().size() != 2 || c.error().empty())
                 return false;
               return c.results()[0].status ==
                          pathfinding::RouteStatus::BackendUnavailable &&
                      c.results()[1].request_id == id;
             });
  n += check("controller rejects child exit without matching report instead of "
             "inventing success",
             [&] {
               PathfindingController c(child, output.path / "bad-worker");
               return c.start(route, {}) && finish(c) && c.results().empty() &&
                      !c.error().empty();
             });
  n += check(
      "controller handles invalid executable without leaving active request",
      [&] {
        PathfindingController c(output.path / "missing.exe",
                                output.path / "missing-worker");
        return !c.start(route, {}) && !c.active() && !c.error().empty();
      });
  return n;
}

#include "PathfindingLoader.h"
#include "PathfindingOverlay.h"
#include "Support.h"
#include <thread>
#include "../navmesh/WorldFixture.h"
using namespace pf_test;
int overlay_tests() {
  int n = 0;
  n += check("region list loads without manual Map and removing input invalidates async generation",[]{
    world_fixture::Directory d;world_fixture::Geometry g;g.floor(32000,0,33536,1024);
    const auto a=world_fixture::save(g,d.path,"20_18"), b=world_fixture::save(g,d.path,"21_18");
    PathfindingLoader loader;
    loader.request({"",{}, {}, {}, {a.mesh_path,b.mesh_path}});
    for(int i=0;i<2000;++i){
      auto result=loader.poll();
      if(result){
        if(!result->dataset || result->dataset->nav_regions().size()!=2)return false;
        loader.request({"",{}, {}, {}, {a.mesh_path}});loader.poll();loader.invalidate();
        while(loader.active()){if(loader.poll())return false;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        loader.request({"",{}, {}, {}, {d.path/"missing.navmesh"}});
        for(int j=0;j<1000;++j){if(auto failure=loader.poll())return !failure->dataset && !failure->error.empty();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
  });
  n += check("navigation polygons retain all visible geometry beyond the old "
             "line budget",
             [] {
               std::vector<std::vector<pathfinding::WorldPoint>> polygons(
                   3000, {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}});
               TopView view;
               const auto lines = navigation_polygon_lines(polygons, view);
               return lines.size() == 12000;
             });
  n += check("navigation polygon Z slice excludes unrelated floors without "
             "clipping the corridor",
             [] {
               std::vector<std::vector<pathfinding::WorldPoint>> polygons{
                   {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}},
                   {{0, 0, 128}, {1, 0, 128}, {1, 1, 128}}};
               TopView view;
               view.z_min = -1;
               view.z_max = 1;
               return navigation_polygon_lines(polygons, view).size() == 3;
             });
  n += check("L2J display scale covers overview and close cells instead of "
             "hiding the grid",
             [] {
               TopView view;
               view.width = 32768;
               const auto far = navigation_cell_width(view, {1024, 768});
               view.width = 1024;
               return far == 256 &&
                      navigation_cell_width(view, {1024, 768}) == 16;
             });
  n += check(
      "validated samples cannot claim current success after endpoints change",
      [] {
        PathfindingContext c;
        c.show_final = false;
        c.show_validated = true;
        c.revision = 1;
        pathfinding::RouteResult r;
        r.segments_valid = true;
        r.validated_samples = {{0, 0, 0}, {1, 1, 0}};
        auto o = route_overlay(c, std::span{&r, 1});
        return o.segments.size() == 1 && o.segments[0].color == 0xff909090u;
      });
  n += check("overlay clips crossing segment to selected Z slice and reports "
             "hidden sections",
             [] {
               PathfindingContext c;
               c.view.z_min = 0;
               c.view.z_max = 10;
               pathfinding::RouteResult r;
               r.backend = "l2j";
               r.final_path = {{0, 0, -10}, {20, 20, 10}, {30, 30, 20}};
               const auto o = route_overlay(c, std::span{&r, 1});
               return o.segments.size() == 2 &&
                      o.segments.front().a ==
                          pathfinding::WorldPoint{10, 10, 0} &&
                      o.outside == 2;
             });
  n += check("overlay preserves raw distinction, uses two colors and greys "
             "stale results",
             [] {
               PathfindingContext c;
               c.show_raw = true;
               std::vector<pathfinding::RouteResult> r(2);
               for (int i = 0; i < 2; ++i) {
                 r[i].backend = i ? "navmesh" : "l2j";
                 r[i].final_path = {{0, 0, 0}, {1, 1, 0}};
                 r[i].raw = r[i].final_path;
               }
               auto o = route_overlay(c, r);
               if (o.segments.size() != 4 ||
                   o.segments[0].color == o.segments[2].color ||
                   !o.segments[0].raw)
                 return false;
               ++c.revision;
               o = route_overlay(c, r);
               return o.segments[0].color == o.segments[2].color;
             });
  n += check("long final route retains every segment through endpoint B", [] {
    PathfindingContext c;
    pathfinding::RouteResult r;
    r.backend = "navmesh";
    for (int i = 0; i < 8407; ++i)
      r.final_path.push_back({double(i), double(i % 3), -3751});
    const auto o = route_overlay(c, std::span{&r, 1});
    if (o.segments.size() != 8406)
      return false;
    for (std::size_t i = 0; i < o.segments.size(); ++i)
      if (o.segments[i].a != r.final_path[i] ||
          o.segments[i].b != r.final_path[i + 1] || o.segments[i].raw)
        return false;
    return o.segments.back().b == pathfinding::WorldPoint{8406, 0, -3751};
  });
  n += check("long raw and validated routes cannot hide either backend's final route", [] {
    PathfindingContext c;
    c.show_raw = true;
    c.show_validated = true;
    std::vector<pathfinding::RouteResult> results(2);
    for (std::size_t backend = 0; backend < results.size(); ++backend) {
      auto &r = results[backend];
      r.backend = backend ? "navmesh" : "l2j";
      for (int i = 0; i < 8407; ++i)
        r.final_path.push_back({double(i), double(backend), 0});
      r.raw = r.validated_samples = r.final_path;
    }
    const auto o = route_overlay(c, results);
    if (o.segments.size() != 50436)
      return false;
    for (std::size_t backend = 0; backend < results.size(); ++backend) {
      const auto offset = backend * 25218;
      for (std::size_t route = 0; route < 3; ++route)
        for (std::size_t i = 0; i < 8406; ++i) {
          const auto &s = o.segments[offset + route * 8406 + i];
          if (s.a != results[backend].final_path[i] ||
              s.b != results[backend].final_path[i + 1] ||
              s.raw != (route != 1))
            return false;
        }
    }
    return o.segments[8406].color != o.segments[33624].color;
  });
  n += check("final and validated sample visibility are independent", [] {
    PathfindingContext c;
    c.show_final = false;
    pathfinding::RouteResult r;
    r.final_path = {{0, 0, 0}, {10, 0, 0}};
    r.validated_samples = {{0, 0, 0}, {5, 0, 0}, {10, 0, 0}};
    if (!route_overlay(c, std::span{&r, 1}).segments.empty())
      return false;
    c.show_validated = true;
    return route_overlay(c, std::span{&r, 1}).segments.size() == 2;
  });
  n += check("obsolete async completion ignored and newest request returned",
             [] {
               std::promise<void> release, started;
               auto gate = release.get_future().share();
               auto began = started.get_future();
               PathfindingLoader loader(
                   [&](const PathfindingLoadRequest &r)
                       -> std::shared_ptr<const pathfinding::Dataset> {
                     if (r.map == "old") {
                       started.set_value();
                       gate.wait();
                     }
                     throw std::runtime_error(r.map);
                   });
               loader.request({"old", {}, {}, {}});
               loader.poll();
               if (began.wait_for(std::chrono::seconds(1)) !=
                   std::future_status::ready) {
                 release.set_value();
                 return false;
               }
               loader.request({"new", {}, {}, {}});
               release.set_value();
               for (int i = 0; i < 1000; ++i) {
                 auto r = loader.poll();
                 if (r)
                   return r->error == "new" && !loader.active();
                 std::this_thread::sleep_for(std::chrono::milliseconds(1));
               }
               return false;
             });
  return n;
}

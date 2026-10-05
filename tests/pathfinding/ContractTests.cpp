#include "Support.h"
#include <limits>
#include <pathfinding/Result.h>
#include <territory/PathIO.h>
using namespace pathfinding;
using namespace pf_test;

int contract_tests() {
  TempDirectory temp;
  RouteCase c{"corner",
              "13_24",
              {{-212992.25, 212992.5, -4112}, -4112},
              {{-212960, 212992, -4112}, -4112},
              identify_file(temp.file("13_24.l2j", "abc")),
              identify_file(temp.file("13_24.navmesh", "xyz"))};
  int n = 0;
  RouteCase world=c;
  world.map="22_22"; world.navmesh={};
  world.a={ {66000,132000,-16}, -16, {{"navmesh",-17}} };
  world.b={ {99000,99000,-16}, -16, {{"navmesh",-17}} };
  for(const auto& map:{"22_22","22_21","23_21"})
    world.nav_regions.push_back({map,identify_file(temp.file((std::string(map)+".navmesh").c_str(),"mesh")),
      identify_file(temp.file((std::string(map)+".navmesh.json").c_str(),"metadata"))});
  n += check("schema2 exact three-region case and own Z roundtrip",[&]{
    const auto j=case_json(world);const auto restored=case_from_json(j);
    return j.at("schema")==2 && restored.nav_regions.size()==3 &&
      navigation_regions(restored)==std::vector<std::string>{"22_21","22_22","23_21"} &&
      case_for_backend(restored,"navmesh").a.requested.z==-17 &&
      !backend_scope_supported(restored,"l2j") && backend_scope_supported(restored,"navmesh");
  });
  n += check("world rejects missing quadrant duplicate and ambiguous representations",[&]{
    auto absent=world;absent.b.requested={99000,132000,-16};
    auto duplicate=world;duplicate.nav_regions.push_back(world.nav_regions.front());
    auto ambiguous=world;ambiguous.navmesh=c.navmesh;
    auto incomplete=world;incomplete.nav_regions[1].metadata={};
    return rejects([&]{validate_case(absent);}) && rejects([&]{validate_case(duplicate);}) &&
      rejects([&]{validate_case(ambiguous);}) && rejects([&]{validate_case(incomplete);});
  });
  n += check("schema2 navmesh-only case accepts absent optional l2j",[&]{
    auto j=case_json(world);j.erase("l2j");
    const auto restored=case_from_json(j);
    return restored.l2j.path.empty() && restored.nav_regions.size()==3 &&
      enabled_backends(restored)==std::vector<std::string>{"navmesh"};
  });
  n += check("world input checks include every mesh and completion marker",[&]{
    if(!input_changes(world).empty())return false;
    temp.file("22_21.navmesh.json","changed metadata");
    const auto changes=input_changes(world);
    const auto current=with_current_identities(world);
    return changes.size()==1 && changes.front().path==world.nav_regions[1].metadata.path && input_changes(current).empty();
  });
  n += check("unsupported scope is explicit and has no invented timing",[&]{
    RouteResult r; r.request_id="req";r.case_id="world";r.backend="l2j";
    r.status=RouteStatus::UnsupportedScope;
    const auto j=result_json(r);
    return j.at("status")=="unsupported_scope" && result_from_json(j).metrics.empty();
  });
  n += check("backend-picked floor offset is independent from nearest-poly "
             "snap radius",
             [&] {
               auto grouped = c;
               grouped.a.backend_z = {{"l2j", -4112}, {"navmesh", -4135}};
               grouped.snap_vertical = 8;
               const auto own = case_for_backend(
                   case_from_json(case_json(grouped)), "navmesh");
               return own.a.requested.z == -4135 && own.snap_vertical == 8;
             });
  n += check("backend-specific clicked floors survive save/replay without "
             "altering common XY",
             [&] {
               auto grouped = c;
               grouped.a.backend_z = {{"l2j", -4112}, {"navmesh", -4135}};
               const auto restored = case_from_json(case_json(grouped));
               const auto nav = case_for_backend(restored, "navmesh");
               const auto geo = case_for_backend(restored, "l2j");
               return restored.a.backend_z == grouped.a.backend_z &&
                      nav.a.requested.z == -4135 &&
                      nav.a.confirmed_z == -4135 &&
                      geo.a.requested.z == -4112 &&
                      nav.a.requested.x == c.a.requested.x &&
                      c.a.backend_z.empty();
             });
  n += check("single-format cases preserve absent inputs and verify only "
             "present files",
             [&] {
               auto single = c;
               single.navmesh = {};
               const auto restored = case_from_json(case_json(single));
               return restored.navmesh.path.empty() &&
                      restored.l2j.sha256 == c.l2j.sha256 &&
                      input_changes(restored).empty() &&
                      with_current_identities(restored).navmesh.path.empty();
             });
  n += check("cases require at least one complete navigation identity", [&] {
    auto empty = c;
    empty.l2j = {};
    empty.navmesh = {};
    auto malformed = c;
    malformed.navmesh.path.clear();
    return rejects([&] { validate_case(empty); }) &&
           rejects([&] { validate_case(malformed); });
  });
  n += check("experimental limits and server reference roundtrip independently",
             [&] {
               auto j = case_json(c);
               j["search_settings"] = {{"server_budget_ms", 100},
                                       {"timeout_ms", 5000},
                                       {"nav_max_nodes", 123456},
                                       {"l2j_max_window_cells", 1024}};
               return case_json(case_from_json(j)).at("search_settings") ==
                      j.at("search_settings");
             });
  n += check("unsafe experimental limits are rejected", [&] {
    auto j = case_json(c);
    j["search_settings"] = {{"server_budget_ms", 100},
                            {"timeout_ms", 0},
                            {"nav_max_nodes", 65535},
                            {"l2j_max_window_cells", 512}};
    return rejects([&] { case_from_json(j); });
  });
  n += check(
      "file backend profile resolves portable paths against profile directory",
      [&] {
        const auto file = temp.path / "portable-profile.json";
        territory::write_json_atomic(
            file,
            {{"schema", 1},
             {"java", "runtime/bin/java.exe"},
             {"legacy_classpath", {"legacy/classes", "legacy/lib/engine.jar"}},
             {"legacy_config", "legacy/geodata.json"},
             {"nav_runner_directory", "navmesh/classes"},
             {"detour_jar", "navmesh/detour.jar"}},
            false);
        const auto p = read_profile(file);
        return p.java == temp.path / "runtime/bin/java.exe" &&
               p.legacy_classpath ==
                   std::vector<std::filesystem::path>{
                       temp.path / "legacy/classes",
                       temp.path / "legacy/lib/engine.jar"} &&
               p.legacy_config == temp.path / "legacy/geodata.json" &&
               p.nav_runner_directory == temp.path / "navmesh/classes" &&
               p.detour_jar == temp.path / "navmesh/detour.jar" &&
               profile_from_json(profile_json(p)).java == p.java;
      });
  n += check("portable profile keeps absolute overrides and explicit "
             "unavailable fields",
             [&] {
               const auto file = temp.path / "mixed-profile.json";
               const auto java = temp.path / "external-java.exe";
               territory::write_json_atomic(
                   file,
                   {{"schema", 1},
                    {"java", territory::path_utf8(java)},
                    {"legacy_classpath", Json::array()},
                    {"legacy_config", ""},
                    {"nav_runner_directory", ""},
                    {"detour_jar", ""}},
                   false);
               const auto p = read_profile(file);
               return p.java == java && p.legacy_classpath.empty() &&
                      p.legacy_config.empty() &&
                      p.nav_runner_directory.empty() && p.detour_jar.empty();
             });
  n += check(
      "bundled profile is selected beside executable without replacing a "
      "manual override",
      [&] {
        const auto empty = default_backend_profile_path(temp.path);
        const auto bundle = temp.path / "pathfinding-backend/profile.json";
        std::filesystem::create_directory(bundle.parent_path());
        territory::write_json_atomic(bundle, {{"schema", 1}}, false);
        const auto selected = temp.path / "manual-profile.json";
        return empty.empty() &&
               default_backend_profile_path(temp.path) == bundle &&
               default_backend_profile_path(temp.path, selected) == selected;
      });
  n += check("saved backend baseline rejects changed implementation/config "
             "before execution",
             [&] {
               auto saved = c;
               saved.expected_backends = {{"l2j", std::string(64, 'a')},
                                          {"navmesh", std::string(64, 'b')}};
               const auto restored = case_from_json(case_json(saved));
               if (restored.expected_backends != saved.expected_backends)
                 return false;
               verify_backend_identity(restored, "l2j", std::string(64, 'a'));
               return rejects([&] {
                 verify_backend_identity(restored, "l2j", std::string(64, 'c'));
               });
             });
  n += check("case roundtrip preserves fractional negative world coordinates "
             "and floor",
             [&] {
               auto restored = case_from_json(case_json(c));
               return restored.id == c.id &&
                      restored.a.requested == c.a.requested &&
                      restored.a.confirmed_z == -4112 &&
                      restored.snap_horizontal == 8;
             });
  n += check("source file has actual SHA256 and size", [&] {
    return c.l2j.size == 3 &&
           c.l2j.sha256 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb41"
                           "0ff61f20015ad";
  });
  n += check("case writer publishes new file and refuses overwrite", [&] {
    auto p = temp.path / "case.json";
    write_case_new(c, p);
    return read_case(p).b.requested == c.b.requested &&
           rejects([&] { write_case_new(c, p); });
  });
  n += check("changed source identity rejected", [&] {
    temp.file("13_24.l2j", "abd");
    return rejects([&] { verify_identity(c.l2j); });
  });
  n += check("case rejects cross-region target", [&] {
    auto j = case_json(c);
    j["b"]["requested"] = {180224, 49152, -4112};
    return rejects([&] { case_from_json(j); });
  });
  n += check("case rejects future schema and malformed hash", [&] {
    auto j = case_json(c);
    j["schema"] = 2;
    const bool version = rejects([&] { case_from_json(j); });
    j = case_json(c);
    j["l2j"]["sha256"] = "bad";
    return version && rejects([&] { case_from_json(j); });
  });
  n += check("case rejects nonfinite and excessively large positions", [&] {
    auto broken = c;
    broken.a.requested.z = std::numeric_limits<double>::infinity();
    auto large = c;
    large.b.requested.z = 1e8;
    return rejects([&] { case_json(broken); }) &&
           rejects([&] { case_json(large); });
  });
  RouteResult r;
  r.request_id = "request-1";
  r.case_id = c.id;
  r.backend = "navmesh";
  r.status = RouteStatus::Partial;
  r.a = ResolvedEndpoint{
      c.a.requested, {-212991, 212991, -4110}, true, "poly:42"};
  r.final_path = {r.a->resolved, c.b.requested};
  r.metrics = {{"visited_nodes", nullptr}, {"search_ns", 42}};
  n += check("result retains requested/resolved points, request ID and "
             "unavailable metrics",
             [&] {
               auto rr = result_from_json(result_json(r));
               return rr.request_id == r.request_id && rr.a &&
                      rr.a->resolved == r.a->resolved &&
                      rr.a->requested == c.a.requested && !rr.b &&
                      !rr.segments_valid && rr.status == RouteStatus::Partial &&
                      rr.metrics.at("visited_nodes").is_null() &&
                      rr.final_path.size() == 2;
             });
  n += check("unknown status and malformed route points are rejected", [&] {
    auto j = result_json(r);
    j["status"] = "SUCCESS_MAYBE";
    const auto bad = rejects([&] { result_from_json(j); });
    j = result_json(r);
    j["final_path"] = {{1, 2}};
    return bad && rejects([&] { result_from_json(j); });
  });
  n += check("oversized path arrays rejected before allocation", [&] {
    auto j = result_json(r);
    j["raw"] = Json::array();
    for (int i = 0; i < 1000001; ++i)
      j["raw"].push_back(nullptr);
    return rejects([&] { result_from_json(j); });
  });
  return n;
}
#ifdef PATHFINDING_CONTRACT_TEST_MAIN
int main() { return contract_tests() ? 1 : 0; }
#endif

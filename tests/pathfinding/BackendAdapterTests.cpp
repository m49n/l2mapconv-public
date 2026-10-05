#include "Support.h"
#include <pathfinding/BackendAdapters.h>
using namespace pathfinding;
using namespace pf_test;
int backend_adapter_tests() {
  TempDirectory tmp;
  RouteCase c{"corner",
              "25_19",
              {{166136, 53528, -4112}, -4112},
              {{166136, 53544, -4112}, -4112},
              identify_file(tmp.file("25_19.l2j", "abc")),
              identify_file(tmp.file("25_19.navmesh", "nav"))};
  Json run{{"type", "run"},
           {"fork", 0},
           {"java", "25"},
           {"loadNs", 1000},
           {"dataHashes", "{25_19.l2j=" + c.l2j.sha256 + "}"}};
  Json sample{{"type", "sample"},
              {"caseId", "corner"},
              {"fork", 0},
              {"iteration", 0},
              {"fromX", 166136},
              {"fromY", 53528},
              {"fromZ", -4112},
              {"toX", 166136},
              {"toY", 53544},
              {"toZ", -4112},
              {"status", "DIRECT_VALID"},
              {"endX", 166136},
              {"endY", 53544},
              {"endZ", -4112},
              {"directNs", 100},
              {"astarNs", nullptr},
              {"cleanNs", nullptr},
              {"validateNs", nullptr},
              {"totalNs", 200},
              {"allocatedBytes", nullptr},
              {"gcCountDelta", nullptr},
              {"gcTimeMsDelta", nullptr}};
  Json evidence{{"type", "evidence"},
                {"caseId", "corner"},
                {"fork", 0},
                {"rawWaypoints", "166136,53528,-4112|166136,53544,-4112"},
                {"cleanedWaypoints", "166136,53528,-4112|166136,53544,-4112"},
                {"validatedCells", "166136,53528,-4112|166136,53544,-4112"},
                {"segmentsValid", true},
                {"serverEndpointValid", true},
                {"visitedNodes", nullptr},
                {"budgetConsumed", nullptr}};
  Json summary{{"type", "summary"},
               {"caseId", "corner"},
               {"fork", 0},
               {"status", "DIRECT_VALID"},
               {"legacyStartHeight", -4112},
               {"legacyTargetHeight", -4112}};
  auto jsonl = [&] {
    return run.dump() + "\n" + sample.dump() + "\n" + evidence.dump() + "\n" +
           summary.dump() + "\n";
  };
  int n = 0;
  n += check("legacy measured series excludes warmup and retains each sample", [&] {
    auto meta=run;meta["warmup"]=2000;meta["iterations"]=2;
    auto second=sample;second["iteration"]=1;second["totalNs"]=400;
    auto text=meta.dump()+"\n"+sample.dump()+"\n"+second.dump()+"\n"+evidence.dump()+"\n"+summary.dump()+"\n";
    const auto r=parse_legacy_jsonl(text,c,"req");
    const auto& samples=r.metrics.at("benchmark_samples");
    if(samples.size()!=2 || samples[0].at("total_ns")!=200 || samples[1].at("total_ns")!=400 ||
       !samples[0].at("search_ns").is_null() || r.metrics.at("benchmark_warmup")!=2000) return false;
    return rejects([&]{parse_legacy_jsonl(meta.dump()+"\n"+sample.dump()+"\n"+evidence.dump()+"\n"+summary.dump()+"\n",c,"req");});
  });
  n += check("visited regions split sparse segments at map boundaries",[]{
    const std::vector<WorldPoint> points{{100,100,1},{70000,100,1}};
    const auto visited=visited_navigation_regions(points,{"20_18","21_18","22_18"});
    return visited==std::vector<std::string>{"20_18","21_18","22_18"} &&
      rejects([&]{visited_navigation_regions(points,{"20_18","22_18"});});
  });
  n += check("measured legacy resource reason is not overwritten by successful "
             "evidence rerun",
             [&] {
               auto limited = sample;
               limited["status"] = "UNKNOWN_LEGACY";
               limited["limitReason"] = "search_timeout";
               limited["stopReason"] = "unknown_legacy";
               limited["visitedNodes"] = 123;
               limited["searchTimeoutMs"] = 5000;
               limited["maxWindowCells"] = 1024;
               limited["peakWindowCells"] = 512;
               const auto r = parse_legacy_jsonl(
                   run.dump() + "\n" + limited.dump() + "\n" + evidence.dump() +
                       "\n" + summary.dump() + "\n",
                   c, "req");
               return r.status == RouteStatus::ResourceLimit &&
                      r.metrics.at("limit_reason") == "search_timeout" &&
                      r.metrics.at("visited_nodes") == 123;
             });
  n += check("proven legacy open-list exhaustion is no path, a window cap is a "
             "resource limit",
             [&] {
               auto limited = sample;
               limited["status"] = "UNKNOWN_LEGACY";
               limited["limitReason"] = "";
               limited["stopReason"] = "exhausted";
               auto parse = [&] {
                 return parse_legacy_jsonl(run.dump() + "\n" + limited.dump() +
                                               "\n" + evidence.dump() + "\n" +
                                               summary.dump() + "\n",
                                           c, "req");
               };
               if (parse().status != RouteStatus::NoPath)
                 return false;
               limited["limitReason"] = "window_limit";
               return parse().status == RouteStatus::ResourceLimit;
             });
  n += check("legacy DIRECT_VALID requires endpoint and floor evidence", [&] {
    auto r = parse_legacy_jsonl(jsonl(), c, "req");
    return r.status == RouteStatus::Reached && r.a && r.b &&
           r.a->resolved.z == -4112 && r.final_path.size() == 2 &&
           r.metrics.at("visited_nodes").is_null();
  });
  n += check(
      "actual server uppercase hexadecimal digest matches input identity", [&] {
        auto saved = run;
        std::string hash = c.l2j.sha256;
        std::transform(hash.begin(), hash.end(), hash.begin(),
                       [](unsigned char ch) {
                         return static_cast<char>(std::toupper(ch));
                       });
        run["dataHashes"] = "{25_19.l2j=" + hash + "}";
        try {
          auto r = parse_legacy_jsonl(jsonl(), c, "req");
          run = saved;
          return r.status == RouteStatus::Reached;
        } catch (...) {
          run = saved;
          throw;
        }
      });
  n += check("legacy wrong floor is not reached even when server says valid",
             [&] {
               auto wrong = c;
               wrong.b.confirmed_z = -3984;
               return parse_legacy_jsonl(jsonl(), wrong, "req").status ==
                      RouteStatus::LayerMismatch;
             });
  n += check("legacy missing selected start height stays unknown, not invented",
             [&] {
               auto saved = summary;
               summary.erase("legacyStartHeight");
               auto r = parse_legacy_jsonl(jsonl(), c, "req");
               summary = saved;
               return !r.a && r.status != RouteStatus::Reached;
             });
  n += check("legacy null and invalid evidence are not no-path proof", [&] {
    auto saved = sample;
    sample["status"] = "UNKNOWN_LEGACY";
    auto r = parse_legacy_jsonl(jsonl(), c, "req");
    sample["status"] = "ASTAR_INVALID";
    auto invalid = parse_legacy_jsonl(jsonl(), c, "req");
    sample = saved;
    return r.status == RouteStatus::UnknownLegacy &&
           invalid.status != RouteStatus::Reached &&
           invalid.status != RouteStatus::NoPath;
  });
  n += check("legacy truncated or wrong-case evidence rejected", [&] {
    auto saved = evidence;
    evidence["caseId"] = "other";
    const auto wrong = rejects([&] { parse_legacy_jsonl(jsonl(), c, "req"); });
    evidence = saved;
    return wrong && rejects([&] {
             parse_legacy_jsonl(run.dump() + "\n" + sample.dump(), c, "req");
           });
  });
  n += check(
      "legacy CSV rounds half away from zero and honors configured geo origin",
      [&] {
        auto fraction = c;
        fraction.a.requested = {166136.5, 53528.5, -4112.5};
        const auto csv = legacy_case_csv(fraction, 11, 10);
        return csv.find(
                   "corner,corner,mouse_click,A,166137,53529,-4113,0,28815,"
                   "19729,0,B,166136,53544,-4112,0,28815,19730,0,editor\n") !=
               std::string::npos;
      });
  RouteResult nav;
  nav.request_id = "req";
  nav.case_id = c.id;
  nav.backend = "navmesh";
  nav.status = RouteStatus::Reached;
  nav.a = ResolvedEndpoint{c.a.requested, c.a.requested, true, "nav:1"};
  nav.b = ResolvedEndpoint{c.b.requested, c.b.requested, true, "nav:2"};
  nav.final_path = {c.a.requested, c.b.requested};
  nav.validated_samples = nav.final_path;
  nav.segments_valid = true;
  n += check("nav adapter rejects stale IDs and oversized snap", [&] {
    auto j = result_json(nav);
    j["request_id"] = "old";
    const bool stale = rejects([&] { parse_navmesh_json(j, c, "req"); });
    auto broken = nav;
    broken.a->resolved.x += 16;
    return stale && parse_navmesh_json(result_json(broken), c, "req").status ==
                        RouteStatus::InvalidEndpoint;
  });
  n += check("partial nav corridor never promoted to reached", [&] {
    auto partial = nav;
    partial.status = RouteStatus::Partial;
    return parse_navmesh_json(result_json(partial), c, "req").status ==
           RouteStatus::Partial;
  });
  n += check(
      "nav endpoint warnings preserve unsuccessful backend evidence", [&] {
        for (auto status : {RouteStatus::Failed, RouteStatus::ResourceLimit,
                            RouteStatus::Partial, RouteStatus::NoPath,
                            RouteStatus::InvalidEndpoint}) {
          for (bool outside_snap : {false, true}) {
            auto failed = nav;
            failed.status = status;
            failed.diagnostic = "Original backend reason";
            failed.b->resolved.z += 8;
            if (outside_snap)
              failed.a->resolved.x += 16;
            failed.final_path.clear();
            failed.validated_samples.clear();
            failed.segments_valid = false;
            const auto parsed =
                parse_navmesh_json(result_json(failed), c, "req");
            if (parsed.status != status ||
                !parsed.diagnostic.starts_with("Original backend reason") ||
                parsed.diagnostic.find("different floor") ==
                    std::string::npos ||
                (outside_snap &&
                 parsed.diagnostic.find("snap limits") == std::string::npos))
              return false;
          }
        }
        return true;
      });
  n += check("nav floor mismatch distinct from nearest-poly snap radius", [&] {
    auto wrong = nav;
    wrong.b->resolved.z += 8;
    return parse_navmesh_json(result_json(wrong), c, "req").status ==
           RouteStatus::LayerMismatch;
  });
  n += check("invalid endpoint takes priority over the other endpoint floor "
             "mismatch in either direction",
             [&] {
               for (bool reverse : {false, true}) {
                 auto mixed = nav;
                 auto &wrong_floor = reverse ? mixed.b : mixed.a;
                 auto &invalid = reverse ? mixed.a : mixed.b;
                 wrong_floor->resolved.z += 8;
                 invalid->valid = false;
                 mixed.status = RouteStatus::InvalidEndpoint;
                 mixed.final_path.clear();
                 mixed.validated_samples.clear();
                 mixed.segments_valid.reset();
                 if (parse_navmesh_json(result_json(mixed), c, "req").status !=
                     RouteStatus::InvalidEndpoint)
                   return false;
               }
               return true;
             });
  n += check(
      "an invalid first endpoint cannot hide a changed second requested point",
      [&] {
        auto mixed = nav;
        mixed.a->valid = false;
        mixed.b->requested.x += 1;
        return rejects(
            [&] { parse_navmesh_json(result_json(mixed), c, "req"); });
      });
  return n;
}

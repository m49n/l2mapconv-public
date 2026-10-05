#include "PathfindingJob.h"
#include "Support.h"
#include <territory/PathIO.h>
using namespace pathfinding;
using namespace pf_test;
int job_tests() {
  TempDirectory input, output;
  RouteCase route{"job-case",
                  "25_19",
                  {{166136, 53528, -4112}, -4112},
                  {{166136, 53544, -4112}, -4112},
                  identify_file(input.file("25_19.l2j", "abc")),
                  identify_file(input.file("25_19.navmesh", "nav"))};
  BackendProfile profile{};
  int n = 0;
  n += check("benchmark progress rejects stale identity and impossible counts", [&] {
    auto job=make_pathfinding_job(route,profile,output.path/"progress",BenchmarkSettings{3,2});
    Json p{{"request_id",job.id},{"backend","navmesh"},{"phase","warmup"},{"completed",2},{"total",3},{"pid",42}};
    if(pathfinding_benchmark_progress(p,job)!=p || pathfinding_job_wall_seconds(job)<=120)return false;
    if(!rejects([&]{pathfinding_benchmark_progress(p,job,43);}))return false;
    auto bad=p;bad["request_id"]="old";
    if(!rejects([&]{pathfinding_benchmark_progress(bad,job);}))return false;
    bad=p;bad["completed"]=4;
    if(!rejects([&]{pathfinding_benchmark_progress(bad,job);}))return false;
    bad=p;bad["completed"]=0.5;
    return rejects([&]{pathfinding_benchmark_progress(bad,job);});
  });
  n += check("benchmark statistics keep null timers, failures and measured count honest", [] {
    Json samples=Json::array({
      {{"status","reached"},{"search_ns",10},{"total_ns",100}},
      {{"status","resource_limit"},{"search_ns",30},{"total_ns",300},{"limit_reason","search_timeout"}},
      {{"status","DIRECT_VALID"},{"search_ns",nullptr},{"total_ns",200}},
      {{"status","reached"},{"search_ns",20},{"total_ns",400}}});
    const auto s=summarize_benchmark(samples,{2000,4});
    if(s.at("search_ns").at("count")!=3 || s.at("search_ns").at("median_ns")!=20 ||
       s.at("search_ns").at("p95_ns")!=30 || s.at("search_ns").at("p99_ns")!=30 ||
       s.at("total_ns").at("median_ns")!=250 || s.at("limit_counts").at("search_timeout")!=1 ||
       s.at("status_counts").at("reached")!=2) return false;
    if(!rejects([&]{summarize_benchmark(samples,{0,5});}))return false;
    samples[0]["total_ns"]=-1;
    return rejects([&]{summarize_benchmark(samples,{0,4});});
  });
  n += check("benchmark job preserves warmup and measurement counts without changing old jobs", [&] {
    auto job = make_pathfinding_job(route, profile, output.path / "benchmark-contract");
    auto j = pathfinding_job_json(job);
    if (j.at("schema") != 1 || j.contains("benchmark")) return false;
    j["schema"] = 2;
    j["benchmark"] = {{"warmup", 2000}, {"iterations", 100}};
    const auto restored = pathfinding_job_from_json(j, job.directory);
    if (pathfinding_job_json(restored) != j) return false;
    for (const auto &bad : {Json(-1), Json(10001), Json(0.5), Json(true)}) {
      auto invalid = j; invalid["benchmark"]["warmup"] = bad;
      if (!rejects([&] { pathfinding_job_from_json(invalid, job.directory); })) return false;
    }
    for (const auto &bad : {Json(0), Json(1001), Json(0.5), Json(true)}) {
      auto invalid = j; invalid["benchmark"]["iterations"] = bad;
      if (!rejects([&] { pathfinding_job_from_json(invalid, job.directory); })) return false;
    }
    j["benchmark"]["warmup"] = 0;
    return pathfinding_job_json(pathfinding_job_from_json(j, job.directory)) == j;
  });
  n += check("world job skips unsupported L2J before requiring Java",[&]{
    auto world=route;world.navmesh={};world.map="22_22";
    world.a={ {66000,132000,0},0 };world.b={ {99000,99000,0},0 };
    for(const auto& map:{"22_22","22_21","23_21"})
      world.nav_regions.push_back({map,identify_file(input.file((std::string(map)+".navmesh").c_str(),"mesh")),
        identify_file(input.file((std::string(map)+".navmesh.json").c_str(),"metadata"))});
    auto job=make_pathfinding_job(world,profile,output.path/"world");
    run_pathfinding_job_file(job.directory);
    const auto r=read_pathfinding_report(job);
    return r.size()==2 && r[0].status==RouteStatus::UnsupportedScope && r[0].metrics.at("search_ns").is_null() &&
      r[1].status==RouteStatus::BackendUnavailable && !std::filesystem::exists(job.directory/"l2j.log") &&
      rejects([&]{make_pathfinding_job(world,profile,input.path/"world-output");});
  });
  n += check("single-format worker reports only the requested backend", [&] {
    auto single = route;
    single.l2j = {};
    auto job = make_pathfinding_job(single, profile, output.path / "nav-only");
    const auto code = run_pathfinding_job_file(job.directory);
    const auto results = read_pathfinding_report(job);
    return code == 3 && results.size() == 1 &&
           results[0].backend == "navmesh" &&
           results[0].status == RouteStatus::BackendUnavailable;
  });
  n += check(
      "cooperative timeout marker is resource limit, not user cancellation",
      [&] {
        auto job =
            make_pathfinding_job(route, profile, output.path / "timeout");
        territory::write_json_atomic(job.directory / "timeout.request",
                                     {{"reason", "wall_limit"}}, false);
        const auto code = run_pathfinding_job_file(job.directory);
        const auto r = read_pathfinding_report(job);
        return code == 3 && r.size() == 2 &&
               r[0].status == RouteStatus::ResourceLimit &&
               r[1].status == RouteStatus::ResourceLimit;
      });
  n += check(
      "job protocol persists exactly the requested pair and immutable profile",
      [&] {
        auto job = make_pathfinding_job(route, profile, output.path / "one");
        auto restored = pathfinding_job_from_json(
            territory::read_json(job.directory / "request.json"),
            job.directory);
        return restored.route.a.requested == route.a.requested &&
               restored.id == job.id && restored.directory == job.directory;
      });
  n += check(
      "job directory cannot be reused or created under input folder", [&] {
        return rejects([&] {
                 make_pathfinding_job(route, profile, output.path / "one");
               }) &&
               rejects([&] {
                 make_pathfinding_job(route, profile, input.path / "job");
               });
      });
  n += check("missing backend produces diagnostic results and cannot overwrite "
             "on repeated invocation",
             [&] {
               auto job = make_pathfinding_job(route, profile,
                                               output.path / "missing-java");
               int code = run_pathfinding_job_file(job.directory);
               auto results = read_pathfinding_report(job);
               if (code != 3 || results.size() != 2 ||
                   results[0].status != RouteStatus::BackendUnavailable ||
                   results[1].status != RouteStatus::BackendUnavailable)
                 return false;
               const auto before = identify_file(job.directory / "report.json");
               return run_pathfinding_job_file(job.directory) == 2 &&
                      identify_file(before.path).sha256 == before.sha256;
             });
  n += check(
      "pre-cancelled job exits cancelled without launching a backend", [&] {
        auto job =
            make_pathfinding_job(route, profile, output.path / "cancelled");
        if (job.directory.empty())
          return false;
        territory::request_cancel(job.directory);
        const int code = run_pathfinding_job_file(job.directory);
        auto r = read_pathfinding_report(job);
        return code == 130 && r.size() == 2 &&
               r[0].status == RouteStatus::Cancelled &&
               r[1].status == RouteStatus::Cancelled;
      });
  n += check(
      "stale report IDs and truncated outputs never become current results",
      [&] {
        auto job = make_pathfinding_job(route, profile, output.path / "stale");
        if (job.directory.empty())
          return false;
        territory::write_json_atomic(
            job.directory / "report.json",
            {{"schema", 1}, {"job_id", "older"}, {"results", Json::array()}},
            false);
        return rejects([&] { read_pathfinding_report(job); });
      });
  n += check("unknown job schema and relative worker path rejected", [&] {
    auto job = make_pathfinding_job(route, profile, output.path / "schema");
    auto j = pathfinding_job_json(job);
    j["schema"] = 99;
    return rejects([&] { pathfinding_job_from_json(j, job.directory); }) &&
           run_pathfinding_job_file("relative") == 2;
  });
  return n;
}

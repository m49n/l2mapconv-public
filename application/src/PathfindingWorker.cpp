#include "OwnedChildProcess.h"
#include "PathfindingJob.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <navmesh/Navmesh.h>
#include <navmesh/WorldMesh.h>
#include <regex>
#include <territory/PathIO.h>
#include <thread>

using namespace pathfinding;
namespace {
using Clock = std::chrono::steady_clock;
struct Cancelled : std::exception {};
struct Timeout : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Unavailable : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct Unsupported : std::runtime_error { using std::runtime_error::runtime_error; };
void verify_inputs(const RouteCase& c,const std::string& backend) {
  if(backend=="l2j"){verify_identity(c.l2j);return;}
  for(const auto& r:navmesh_region_inputs(c)) {
    verify_identity(r.mesh);if(has_input(r.metadata))verify_identity(r.metadata);
  }
}
std::uint64_t elapsed(Clock::time_point since) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() -
                                                              since)
      .count();
}
std::string read_text(const std::filesystem::path &p) {
  const auto size = std::filesystem::file_size(p);
  if (size > 16 * 1024 * 1024)
    throw std::invalid_argument("Backend text exceeds 16 MiB limit");
  std::ifstream in(p, std::ios::binary);
  std::string text(static_cast<std::size_t>(size), '\0');
  in.read(text.data(), static_cast<std::streamsize>(size));
  if (!in || in.peek() != std::char_traits<char>::eof())
    throw std::runtime_error("Incomplete backend text");
  return text;
}
void check(const PathfindingJob &job, Clock::time_point deadline) {
  if (std::filesystem::exists(job.directory / "timeout.request"))
    throw Timeout{"Pathfinding controller exceeded job wall limit"};
  if (territory::cancellation_requested(job.directory))
    throw Cancelled{};
  if (Clock::now() >= deadline)
    throw Timeout{"Pathfinding job exceeded job wall limit"};
}
void backend_available(const BackendProfile &p, const std::string &backend) {
  if (p.java.empty() || !std::filesystem::is_regular_file(p.java))
    throw Unavailable{"Select a Java executable in the backend profile"};
  if (backend == "l2j") {
    if (p.legacy_classpath.empty() || p.legacy_config.empty() ||
        !std::filesystem::is_regular_file(p.legacy_config))
      throw Unavailable{"Legacy server classpath/configuration unavailable"};
    for (const auto &entry : p.legacy_classpath)
      if (!std::filesystem::exists(entry))
        throw Unavailable{"Legacy classpath entry missing"};
  } else {
    if (p.nav_runner_directory.empty() ||
        !std::filesystem::is_regular_file(p.nav_runner_directory /
                                          "NavmeshRouteRunner.class") ||
        p.detour_jar.empty() || !std::filesystem::is_regular_file(p.detour_jar))
      throw Unavailable{"Prepare the local recast4j runner profile first"};
    if (identify_file(p.detour_jar).sha256 !=
        "bcd283f7daf0605136be979bf9322ed90f408784abeabf8fb1df34a76714dac6")
      throw Unavailable{"recast4j JAR does not match pinned version/hash"};
  }
}
Json backend_identity(const PathfindingJob &job, const std::string &backend,
                      Clock::time_point deadline) {
  std::vector<std::filesystem::path> roots{job.profile.java};
  if (backend == "l2j") {
    roots.push_back(job.profile.legacy_config);
    roots.insert(roots.end(), job.profile.legacy_classpath.begin(),
                 job.profile.legacy_classpath.end());
  } else {
    roots.push_back(job.profile.detour_jar);
    roots.push_back(job.profile.nav_runner_directory);
  }
  auto entries = Json::array();
  std::size_t count = 0;
  for (const auto &root : roots) {
    check(job, deadline);
    if (std::filesystem::is_directory(root)) {
      std::vector<std::filesystem::path> files;
      for (const auto &entry :
           std::filesystem::recursive_directory_iterator(root)) {
        check(job, deadline);
        if (entry.is_symlink())
          throw std::invalid_argument(
              "Backend class directories must not contain symlinks");
        if (entry.is_regular_file()) {
          if (++count > 100000)
            throw std::invalid_argument(
                "Backend identity exceeds file-count limit");
          files.push_back(entry.path());
        }
      }
      std::sort(files.begin(), files.end());
      auto hashes = Json::array();
      for (const auto &file : files) {
        check(job, deadline);
        hashes.push_back(identity_json(identify_file(file)));
      }
      entries.push_back({{"directory", territory::path_utf8(
                                           std::filesystem::canonical(root))},
                         {"files", hashes}});
    } else
      entries.push_back(identity_json(identify_file(root)));
  }
  return {{"backend", backend},
          {"artifacts", entries},
          {"legacy_experimental_limits",
           backend == "l2j" && job.profile.legacy_experimental_limits}};
}
std::wstring wide(const std::string &text) {
  return territory::path_from_utf8(text).wstring();
}
std::wstring xyz(WorldPoint p) {
  // dump retains the original double precision; only L2J rounds at its adapter.
  return wide(Json(p.x).dump() + "," + Json(p.y).dump() + "," +
              Json(p.z).dump());
}
std::wstring classpath(const std::vector<std::filesystem::path> &paths) {
  std::wstring cp;
  for (const auto &p : paths) {
    const auto s = p.wstring();
    if (s.find(L';') != std::wstring::npos)
      throw std::invalid_argument("Semicolon in classpath entry");
    if (!cp.empty())
      cp += L';';
    cp += s;
  }
  return cp;
}
int config_integer(const std::string &config, const std::string &name) {
  // Match the existing server reader, including quoted values and #-comments.
  const std::regex pattern("(?:^|\\n)[ \\t]*\"" + name +
                           "\"[ \\t]*:[ \\t]*\"([0-9]+)\"");
  std::smatch found;
  if (!std::regex_search(config, found, pattern))
    throw std::invalid_argument("Missing server config key " + name);
  return std::stoi(found[1]);
}
void run_child(const PathfindingJob &job, std::vector<std::wstring> args,
               const std::string &backend, Clock::time_point deadline) {
  if(job.benchmark) {
    args.insert(args.begin(), {L"-Dl2mapconv.progress="+(job.directory/"benchmark-progress.json").wstring(),
                              L"-Dl2mapconv.requestId="+wide(job.id),L"-Dl2mapconv.backend="+wide(backend)});
    if(backend=="navmesh")args.insert(args.end(),{L"--warmup",std::to_wstring(job.benchmark->warmup),
                                                 L"--iterations",std::to_wstring(job.benchmark->iterations)});
  }
  OwnedChildProcess process;
  process.start(job.profile.java, args, job.directory / (backend + ".log"));
  const auto child_deadline =
      std::min(deadline, Clock::now() + std::chrono::seconds(job.benchmark?600:60));
  while (!process.exit_code()) {
    check(job, deadline);
    if (Clock::now() >= child_deadline)
      throw Timeout{job.benchmark?"Benchmark exceeded 600-second backend wall limit; no complete series":"Backend exceeded 60-second wall limit"};
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  if (*process.exit_code() != 0)
    throw std::runtime_error("Backend exited with code " +
                             std::to_string(*process.exit_code()) + "; see " +
                             backend + ".log");
}
RouteResult invoke(const PathfindingJob &job, const std::string &backend,
                   Clock::time_point deadline) {
  const auto c = case_for_backend(job.route, backend);
  const auto &p = job.profile;
  verify_inputs(c,backend);
  if (backend == "navmesh") {
    auto mesh_input=c.navmesh;
    const auto preparation=Clock::now();
    std::uint64_t prepare_ns=0;
    auto region_identities=Json::array();
    if(!c.nav_regions.empty()) {
      std::vector<navmesh::RegionFile> files;
      for(const auto& r:c.nav_regions){
        files.push_back({r.map,r.mesh.path});
        region_identities.push_back({{"map",r.map},{"mesh",identity_json(r.mesh)},{"metadata",identity_json(r.metadata)}});
      }
      const auto cancel=[&]{check(job,deadline);return false;};
      auto world=navmesh::load_world(files,cancel);
      const auto path=job.directory/"world.navmesh";
      navmesh::save(*world.mesh,path,cancel);
      mesh_input=identify_file(path);
      verify_inputs(c,backend);
      prepare_ns=elapsed(preparation);
    }
    // Validate the complete native blob before Java interprets its raw arrays.
    const auto validated = navmesh::load(mesh_input.path);
    (void)validated;
    const auto output = job.directory / "navmesh.json";
    run_child(job,
              {L"-Xmx1024m",
               L"-cp",
               classpath({p.nav_runner_directory, p.detour_jar}),
               L"NavmeshRouteRunner",
               L"--mesh",
               mesh_input.path.wstring(),
               L"--request-id",
               wide(job.id),
               L"--case-id",
               wide(c.id),
               L"--from",
               xyz(c.a.requested),
               L"--to",
               xyz(c.b.requested),
               L"--snap-horizontal",
               wide(Json(c.snap_horizontal).dump()),
               L"--snap-vertical",
               wide(Json(c.snap_vertical).dump()),
               L"--search-timeout-ms",
               std::to_wstring(c.search.timeout_ms),
               L"--max-nodes",
               std::to_wstring(c.search.nav_max_nodes),
               L"--output",
               output.wstring()},
              backend, deadline);
    auto result = parse_navmesh_json(territory::read_json(output), c, job.id);
    if (result.identity.at("mesh_sha256") != mesh_input.sha256)
      throw std::runtime_error("Java loaded different navmesh bytes");
    verify_identity(mesh_input);
    if(!c.nav_regions.empty()) {
      result.identity["nav_regions"]=region_identities;
      result.identity["merged_mesh"]=identity_json(mesh_input);
      result.metrics["prepare_ns"]=prepare_ns;
      result.metrics["merged_load_ns"]=result.metrics.value("load_ns",Json(nullptr));
    }
    std::vector<std::string> maps;
    for(const auto& r:navmesh_region_inputs(c))maps.push_back(r.map);
    if(result.segments_valid.value_or(false))result.metrics["visited_regions"]=visited_navigation_regions(result.final_path,maps);
    return result;
  }
  const auto config = read_text(p.legacy_config);
  const auto csv = legacy_case_csv(c, config_integer(config, "GeoFirstX"),
                                   config_integer(config, "GeoFirstY"));
  const auto data = job.directory / "legacy-data";
  if (!std::filesystem::create_directory(data))
    throw std::runtime_error("Legacy data directory already exists");
  std::filesystem::copy_file(c.l2j.path, data / (c.map + ".l2j"));
  if (identify_file(data / (c.map + ".l2j")).sha256 != c.l2j.sha256)
    throw std::runtime_error("L2J changed while staging");
  const auto cases = job.directory / "legacy-cases.csv";
  {
    std::ofstream out(cases, std::ios::binary);
    out << csv;
    out.close();
    if (!out)
      throw std::runtime_error("Cannot write legacy case CSV");
  }
  const auto output = job.directory / "legacy.jsonl";
  run_child(
      job,
      {L"-Xmx1024m",
       L"-Dl2mapconv.searchTimeoutMs=" + std::to_wstring(c.search.timeout_ms),
       L"-Dl2mapconv.l2jMaxWindowCells=" +
           std::to_wstring(c.search.l2j_max_window_cells),
       L"-cp",
       classpath(p.legacy_classpath),
       L"org.mmocore.gameserver.geoengine.benchmark.GeoRouteBenchmark",
       L"--data-dir",
       data.wstring(),
       L"--config",
       p.legacy_config.wstring(),
       L"--cases",
       cases.wstring(),
       L"--output",
       output.wstring(),
       L"--warmup",
       std::to_wstring(job.benchmark?job.benchmark->warmup:0),
       L"--iterations",
       std::to_wstring(job.benchmark?job.benchmark->iterations:1),
       L"--forks",
       L"1",
       L"--heap",
       L"1024m",
       L"--seed",
       L"542"},
      backend, deadline);
  auto result = parse_legacy_jsonl(read_text(output), c, job.id);
  result.metrics["experimental_limits_supported"] =
      p.legacy_experimental_limits;
  if (!p.legacy_experimental_limits) {
    if (!result.diagnostic.empty())
      result.diagnostic += "; ";
    result.diagnostic += "Legacy backend lacks experimental controls; its "
                         "original search limits apply";
  }
  return result;
}
RouteResult failure(const PathfindingJob &job, const std::string &backend,
                    RouteStatus status, const std::string &message) {
  RouteResult r;
  r.request_id = job.id;
  r.case_id = job.route.id;
  r.backend = backend;
  r.status = status;
  r.diagnostic = message;
  r.metrics = {{"load_ns", nullptr},
               {"search_ns", nullptr},
               {"smoothing_ns", nullptr},
               {"validation_ns", nullptr},
               {"visited_nodes", nullptr}};
  return r;
}
} // namespace
int run_pathfinding_job_file(const std::filesystem::path &directory) {
  try {
    if (!directory.is_absolute())
      throw std::invalid_argument("Pathfinding job directory must be absolute");
    const auto job = pathfinding_job_from_json(
        territory::read_json(directory / "request.json"), directory);
    const auto request = identify_file(directory / "request.json");
    territory::write_json_atomic(
        directory / "started.json",
        {{"job_id", job.id}, {"request_sha256", request.sha256}}, false);
    const auto began = Clock::now(),
               deadline = began + std::chrono::seconds(pathfinding_job_wall_seconds(job));
    territory::Status status;
    status.job_id = job.id;
    status.map = job.route.map;
    const auto enabled = enabled_backends(job.route);
    status.map_count = enabled.size();
    std::string current_backend;
    const auto publish = [&] {
      auto j = territory::to_json(status);
      j["backend"] = current_backend;
      territory::write_json_atomic(directory / "status.json", j, true);
    };
    std::vector<RouteResult> results;
    bool cancelled = false;
    publish();
    for (const auto &backend : enabled) {
      current_backend = backend;
      status.phase = territory::Phase::Loading;
      publish();
      RouteResult result;
      const auto backend_start = Clock::now();
      try {
        check(job, deadline);
        if(!backend_scope_supported(job.route,backend))
          throw Unsupported{backend=="l2j"?"L2J supports endpoints within its single loaded region only; use Navmesh for this scope":"Endpoints are outside the loaded Navmesh region set"};
        backend_available(job.profile, backend);
        const auto hash_start = Clock::now();
        const auto before = backend_identity(job, backend, deadline);
        const auto manifest = directory / (backend + "-identity.json");
        territory::write_json_atomic(manifest, before, false);
        const auto manifest_hash = identify_file(manifest);
        verify_backend_identity(job.route, backend, manifest_hash.sha256);
        const auto hash_ns = elapsed(hash_start);
        status.phase = territory::Phase::Rendering;
        publish();
        const auto invocation = Clock::now();
        result = invoke(job, backend, deadline);
        if(job.benchmark) {
          if(result.metrics.at("benchmark_warmup")!=job.benchmark->warmup)
            throw std::invalid_argument("Backend warmup count differs from request");
          result.metrics["benchmark"]=summarize_benchmark(result.metrics.at("benchmark_samples"),*job.benchmark);
        } else {
          result.metrics.erase("benchmark_samples");result.metrics.erase("benchmark_warmup");
        }
        const auto invocation_ns = elapsed(invocation);
        check(job, deadline);
        verify_identity(request);
        verify_inputs(job.route,backend);
        if (backend_identity(job, backend, deadline) != before)
          throw std::runtime_error("Backend artifacts changed during query");
        result.metrics["backend_wall_ns"] = invocation_ns;
        result.metrics["identity_ns"] = hash_ns;
        result.identity["backend_manifest"] = identity_json(manifest_hash);
        result.identity["request_sha256"] = request.sha256;
      } catch (const Unsupported &e) {
        result=failure(job,backend,RouteStatus::UnsupportedScope,e.what());
      } catch (const Cancelled &) {
        cancelled = true;
        result =
            failure(job, backend, RouteStatus::Cancelled, "Cancelled by user");
      } catch (const Timeout &e) {
        result = failure(job, backend, RouteStatus::ResourceLimit, e.what());
      } catch (const Unavailable &e) {
        result =
            failure(job, backend, RouteStatus::BackendUnavailable, e.what());
      } catch (const std::invalid_argument &e) {
        result = failure(job, backend, RouteStatus::InvalidData, e.what());
      } catch (const std::exception &e) {
        result = failure(job, backend, RouteStatus::Failed, e.what());
      }
      result.metrics["worker_backend_wall_ns"] = elapsed(backend_start);
      result.metrics["server_budget_ms"] = job.route.search.server_budget_ms;
      result.identity["requested_search_settings"] =
          search_settings_json(job.route.search);
      results.push_back(std::move(result));
      ++status.map_index;
    }
    // No backend result can survive inputs changing during the overall request.
    for (auto &r : results) {
      if (r.status == RouteStatus::BackendUnavailable ||
          r.status == RouteStatus::Cancelled || r.status==RouteStatus::UnsupportedScope)
        continue;
      try {
        verify_identity(request);
        verify_inputs(job.route,r.backend);
      } catch (const std::exception &e) {
        r = failure(job, r.backend, RouteStatus::InvalidData, e.what());
      }
    }
    auto reports = Json::array();
    bool valid_execution = false;
    for (const auto &r : results) {
      reports.push_back(result_json(r));
      valid_execution |= r.status == RouteStatus::Reached ||
                         r.status == RouteStatus::Partial ||
                         r.status == RouteStatus::NoPath ||
                         r.status == RouteStatus::UnknownLegacy ||
                         r.status == RouteStatus::LayerMismatch ||
                         r.status == RouteStatus::InvalidEndpoint;
    }
    const int exit_code = cancelled ? 130 : valid_execution ? 0 : 3;
    status.phase = territory::Phase::Saving;
    publish();
    territory::write_json_atomic(directory / "report.json",
                                 {{"schema", 1},
                                  {"job_id", job.id},
                                  {"request_sha256", request.sha256},
                                  {"case", case_json(job.route)},
                                  {"results", reports},
                                  {"job_wall_ns", elapsed(began)},
                                  {"exit_code", exit_code}},
                                 false);
    status.phase = cancelled         ? territory::Phase::Cancelled
                   : valid_execution ? territory::Phase::Completed
                                     : territory::Phase::Failed;
    if (exit_code)
      status.error = cancelled ? "Cancelled by user"
                               : "Requested navigation backends failed or hit "
                                 "limits; see report.json";
    publish();
    return exit_code;
  } catch (const std::exception &e) {
    std::cerr << "Pathfinding job: " << e.what() << '\n';
    return 2;
  }
}

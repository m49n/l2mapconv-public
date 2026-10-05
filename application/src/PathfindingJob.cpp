#include "PathfindingJob.h"
#include <set>
#include <stdexcept>
#include <territory/PathIO.h>
using namespace pathfinding;
namespace {
void validate(const PathfindingJob &j) {
  validate_case(j.route);
  if (j.benchmark) validate_benchmark(*j.benchmark);
  (void)profile_json(j.profile);
  if (!j.directory.is_absolute() || j.id.empty() || j.id.size() > 128 ||
      territory::path_utf8(j.directory.filename()) != j.id)
    throw std::invalid_argument(
        "Job must have an absolute directory matching its ID");
  std::vector<std::filesystem::path> inputs{j.route.l2j.path,j.route.navmesh.path};
  for(const auto& r:j.route.nav_regions){inputs.push_back(r.mesh.path);inputs.push_back(r.metadata.path);}
  for (const auto &source : inputs)
    if (!source.empty() &&
        territory::is_within(j.directory, source.parent_path()))
      throw std::invalid_argument(
          "Pathfinding output must be outside navigation input directories");
  auto directories = j.profile.legacy_classpath;
  directories.push_back(j.profile.nav_runner_directory);
  for (const auto &source : directories)
    if (!source.empty() && std::filesystem::is_directory(source) &&
        territory::is_within(j.directory, source))
      throw std::invalid_argument(
          "Pathfinding output must be outside backend class directories");
}
} // namespace
auto pathfinding_job_json(const PathfindingJob &j) -> Json {
  validate(j);
  Json result{{"schema", j.benchmark ? 2 : 1},
          {"kind", "pathfinding"},
          {"job_id", j.id},
          {"route", case_json(j.route)},
          {"profile", profile_json(j.profile)}};
  if (j.benchmark) result["benchmark"] = benchmark_json(*j.benchmark);
  return result;
}
auto pathfinding_job_from_json(const Json &j,
                               const std::filesystem::path &directory)
    -> PathfindingJob {
  if ((j.at("schema") != 1 && j.at("schema") != 2) || !j.at("schema").is_number_integer() ||
      j.at("kind") != "pathfinding")
    throw std::invalid_argument("Unsupported pathfinding job schema");
  PathfindingJob job{j.at("job_id").get<std::string>(), directory,
                     case_from_json(j.at("route")),
                     profile_from_json(j.at("profile"))};
  if (j.at("schema") == 2) job.benchmark = benchmark_from_json(j.at("benchmark"));
  else if (j.contains("benchmark")) throw std::invalid_argument("Benchmark requires job schema 2");
  validate(job);
  return job;
}
auto make_pathfinding_job(const RouteCase &route, const BackendProfile &profile,
                          const std::filesystem::path &directory,
                          std::optional<BenchmarkSettings> benchmark)
    -> PathfindingJob {
  PathfindingJob job{territory::path_utf8(directory.filename()), directory,
                     route, profile, benchmark};
  auto json = pathfinding_job_json(job);
  std::filesystem::create_directories(directory.parent_path());
  if (!std::filesystem::create_directory(directory))
    throw std::invalid_argument("Pathfinding job directory already exists");
  territory::write_json_atomic(directory / "request.json", json, false);
  return job;
}
int pathfinding_job_wall_seconds(const PathfindingJob &job) {
  return job.benchmark ? 1260 : 120;
}
Json pathfinding_benchmark_progress(const Json &p, const PathfindingJob &job, std::uint64_t previous_pid) {
  const auto backends=enabled_backends(job.route);
  const auto backend=p.at("backend").get<std::string>();
  const auto phase=p.at("phase").get<std::string>();
  if(!job.benchmark || p.at("request_id")!=job.id ||
     std::find(backends.begin(),backends.end(),backend)==backends.end() ||
     (phase!="warmup" && phase!="measurement") || !p.at("pid").is_number_integer() || p.at("pid")<=0 ||
     p.at("pid")>4294967295ull || (previous_pid && p.at("pid")!=previous_pid) ||
     !p.at("completed").is_number_integer() || !p.at("total").is_number_integer() ||
     p.at("completed")<0 || p.at("completed")>p.at("total") ||
     p.at("total")!=(phase=="warmup"?job.benchmark->warmup:job.benchmark->iterations))
    throw std::invalid_argument("Stale or invalid benchmark progress");
  return p;
}
auto read_pathfinding_report(const PathfindingJob &job)
    -> std::vector<RouteResult> {
  const auto j = territory::read_json(job.directory / "report.json");
  if (j.at("schema") != 1 || j.at("job_id") != job.id ||
      j.at("request_sha256") !=
          identify_file(job.directory / "request.json").sha256 ||
      j.at("case") != case_json(job.route))
    throw std::invalid_argument("Stale or mismatched pathfinding report");
  const auto &results = j.at("results");
  const auto enabled = enabled_backends(job.route);
  if (!results.is_array() || results.size() != enabled.size())
    throw std::invalid_argument("Incomplete pathfinding report");
  std::set<std::string> backends;
  std::vector<RouteResult> parsed;
  for (const auto &v : results) {
    auto result = result_from_json(v);
    if (result.request_id != job.id || result.case_id != job.route.id ||
        std::find(enabled.begin(), enabled.end(), result.backend) ==
            enabled.end() ||
        !backends.insert(result.backend).second)
      throw std::invalid_argument("Stale or duplicate backend result");
    parsed.push_back(std::move(result));
  }
  return parsed;
}

#pragma once
#include <pathfinding/BackendAdapters.h>
#include <pathfinding/Benchmark.h>
#include <territory/Job.h>

struct PathfindingJob {
  std::string id;
  std::filesystem::path directory;
  pathfinding::RouteCase route;
  pathfinding::BackendProfile profile;
  std::optional<pathfinding::BenchmarkSettings> benchmark{};
};
int pathfinding_job_wall_seconds(const PathfindingJob &);
pathfinding::Json pathfinding_benchmark_progress(const pathfinding::Json &, const PathfindingJob &, std::uint64_t previous_pid=0);
auto pathfinding_job_json(const PathfindingJob &) -> pathfinding::Json;
auto pathfinding_job_from_json(const pathfinding::Json &,
                               const std::filesystem::path &) -> PathfindingJob;
auto make_pathfinding_job(const pathfinding::RouteCase &,
                          const pathfinding::BackendProfile &,
                          const std::filesystem::path &new_directory,
                          std::optional<pathfinding::BenchmarkSettings> = {})
    -> PathfindingJob;
auto read_pathfinding_report(const PathfindingJob &)
    -> std::vector<pathfinding::RouteResult>;
int run_pathfinding_job_file(const std::filesystem::path &);

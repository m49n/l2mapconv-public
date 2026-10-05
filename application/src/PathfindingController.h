#pragma once
#include "OwnedChildProcess.h"
#include "PathfindingJob.h"
#include <chrono>

class PathfindingController {
public:
  PathfindingController(std::filesystem::path executable,
                        std::filesystem::path output);
  bool start(const pathfinding::RouteCase &,
             const pathfinding::BackendProfile &,
             std::optional<pathfinding::BenchmarkSettings> = {});
  void poll();
  void cancel();
  bool active() const { return m_running; }
  const std::vector<pathfinding::RouteResult> &results() const {
    return m_results;
  }
  const std::string &request_id() const { return m_job.id; }
  const std::string &error() const { return m_error; }
  const std::filesystem::path &job_directory() const { return m_job.directory; }
  const std::optional<territory::Status> &status() const { return m_status; }
  const pathfinding::RouteCase &submitted_case() const { return m_job.route; }
  const pathfinding::Json &benchmark_progress() const { return m_progress; }

private:
  std::filesystem::path m_executable, m_output;
  PathfindingJob m_job;
  OwnedChildProcess m_process;
  std::vector<pathfinding::RouteResult> m_results;
  std::optional<territory::Status> m_status;
  pathfinding::Json m_progress;
  std::map<std::string,std::uint64_t> m_benchmark_pids;
  std::optional<std::chrono::steady_clock::time_point> m_cancel_deadline;
  std::chrono::steady_clock::time_point m_started;
  bool m_running{}, m_forced{}, m_timed_out{};
  std::string m_error;
  void fail(const std::string &);
};

#include "PathfindingController.h"
#include <random>
#include <territory/PathIO.h>

PathfindingController::PathfindingController(std::filesystem::path exe,
                                             std::filesystem::path output)
    : m_executable(std::move(exe)), m_output(std::move(output)) {}

void PathfindingController::fail(const std::string &message) {
  m_error = message;
  m_results.clear();
  if (m_running) {
    try {
      m_process.terminate_tree_owned();
    } catch (const std::exception &e) {
      m_error += "; " + std::string(e.what());
    }
  }
  m_running = false;
}
bool PathfindingController::start(const pathfinding::RouteCase &route,
                                  const pathfinding::BackendProfile &profile,
                                  std::optional<pathfinding::BenchmarkSettings> benchmark) {
  if (m_running)
    return false;
  m_error.clear();
  m_results.clear();
  m_status.reset();
  m_progress=nullptr;
  m_benchmark_pids.clear();
  m_cancel_deadline.reset();
  m_forced = m_timed_out = false;
  try {
    std::random_device random;
    const auto id = "pathfinding-" + std::to_string(random()) + "-" +
                    std::to_string(random());
    m_job = make_pathfinding_job(route, profile,
                                 std::filesystem::absolute(m_output) / id, benchmark);
    m_process.start(m_executable,
                    {L"--pathfinding-job", m_job.directory.wstring()},
                    m_job.directory / "worker.log");
    m_started = std::chrono::steady_clock::now();
    m_running = true;
    return true;
  } catch (const std::exception &e) {
    fail(e.what());
    return false;
  }
}
void PathfindingController::poll() {
  if (!m_running)
    return;
  try {
    const auto now = std::chrono::steady_clock::now();
    if (now - m_started >= std::chrono::seconds(pathfinding_job_wall_seconds(m_job)) && !m_cancel_deadline) {
      territory::write_json_atomic(m_job.directory / "timeout.request",
                                   {{"reason", "wall_limit"}}, false);
      m_cancel_deadline = now + std::chrono::seconds(5);
      m_timed_out = true;
    }
    if (m_cancel_deadline && now >= *m_cancel_deadline && !m_forced) {
      m_process.terminate_tree_owned();
      m_forced = true;
    }
    const auto code = m_process.exit_code();
    const auto status_path = m_job.directory / "status.json";
    if (!m_forced && std::filesystem::exists(status_path)) {
      const auto status=territory::read_json(status_path);
      m_status = territory::status_from_json(status,m_job.id);
      const auto progress=m_job.directory/"benchmark-progress.json";
      m_progress=nullptr;
      if(m_job.benchmark && std::filesystem::exists(progress)) {
        if(std::filesystem::file_size(progress)>4096)throw std::invalid_argument("Benchmark progress exceeds size limit");
        const auto data=territory::read_json(progress);
        if(data.at("backend")==status.at("backend")) {
          auto& pid=m_benchmark_pids[data.at("backend").get<std::string>()];
          m_progress=pathfinding_benchmark_progress(data,m_job,pid);
          pid=m_progress.at("pid").get<std::uint64_t>();
        }
      }
    }
    if (!code)
      return;
    m_running = false;
    if (m_forced) {
      m_results.clear();
      for (const auto &backend : pathfinding::enabled_backends(m_job.route)) {
        pathfinding::RouteResult r;
        r.request_id = m_job.id;
        r.case_id = m_job.route.id;
        r.backend = backend;
        r.status = m_timed_out ? pathfinding::RouteStatus::ResourceLimit
                               : pathfinding::RouteStatus::Cancelled;
        r.diagnostic =
            m_timed_out ? "Worker exceeded wall limit"
                        : "Cancelled; owned worker stopped after grace period";
        m_results.push_back(std::move(r));
      }
      return;
    }
    m_results = read_pathfinding_report(m_job);
    const auto report = territory::read_json(m_job.directory / "report.json");
    if (report.at("exit_code") != *code || !m_status ||
        (*code == 0 && m_status->phase != territory::Phase::Completed) ||
        (*code == 130 && m_status->phase != territory::Phase::Cancelled) ||
        (*code == 3 && m_status->phase != territory::Phase::Failed) ||
        (*code != 0 && *code != 3 && *code != 130))
      throw std::runtime_error("Worker exit code/status/report disagree");
    if (*code == 3)
      m_error = m_status->error;
  } catch (const std::exception &e) {
    fail(e.what());
  }
}
void PathfindingController::cancel() {
  if (!m_running || m_cancel_deadline)
    return;
  try {
    territory::request_cancel(m_job.directory);
    m_cancel_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
  } catch (const std::exception &e) {
    fail(e.what());
  }
}

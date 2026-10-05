#pragma once

#include "GeodataBuildJob.h"
#include "TerritoryProcess.h"

#include <chrono>

class GeodataBuildController {
public:
  GeodataBuildController(std::filesystem::path executable,
                         std::filesystem::path output,
                         std::unique_ptr<TerritoryProcess>);
  bool start(const std::filesystem::path &client,
             const std::vector<std::string> &maps,
             const geodata::BuilderSettings &, bool client_dat, bool l2j = true,
             bool navmesh = false, const navmesh::Settings & = {},
             bool navmesh_neighbor_context = false);
  void poll();
  void cancel();
  bool active() const { return m_running; }
  const std::optional<territory::Status> &status() const { return m_status; }
  const std::filesystem::path &job_directory() const { return m_job.directory; }
  const std::filesystem::path &output_root() const { return m_output; }
  const std::string &error() const { return m_error; }
  const std::string &format() const { return m_format; }

private:
  std::filesystem::path m_executable, m_output;
  std::unique_ptr<TerritoryProcess> m_process;
  GeodataBuildJob m_job;
  std::optional<territory::Status> m_status;
  std::optional<std::chrono::steady_clock::time_point> m_cancel_deadline;
  std::string m_error, m_format;
  bool m_running{}, m_forced{}, m_failed{};
  void fail(const std::string &);
};

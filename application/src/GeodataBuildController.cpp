#include "GeodataBuildController.h"

#include <algorithm>
#include <territory/PathIO.h>
#include <navmesh/RegionMetadata.h>

GeodataBuildController::GeodataBuildController(
    std::filesystem::path executable, std::filesystem::path output,
    std::unique_ptr<TerritoryProcess> process)
    : m_executable{std::move(executable)}, m_output{std::move(output)},
      m_process{std::move(process)} {
  if (!m_process)
    throw std::invalid_argument("Geodata build requires a worker process");
}

void GeodataBuildController::fail(const std::string &message) {
  m_failed = true;
  m_error = message;
  if (m_status) {
    m_status->phase = territory::Phase::Failed;
    m_status->error = message;
  }
}

bool GeodataBuildController::start(const std::filesystem::path &client,
                                   const std::vector<std::string> &maps,
                                   const geodata::BuilderSettings &settings,
                                   bool client_dat, bool l2j, bool navmesh,
                                   const navmesh::Settings &navmesh_settings,
                                   bool navmesh_neighbor_context) {
  if (m_running)
    return false;
  m_error.clear();
  m_format.clear();
  m_status.reset();
  m_cancel_deadline.reset();
  m_forced = m_failed = false;
  m_job = {};
  try {
    GeodataBuildJob candidate{"pending",
                              std::filesystem::absolute(m_output) / "pending",
                              client,
                              maps,
                              settings,
                              client_dat,
                              l2j,
                              navmesh,
                              navmesh_settings,
                              navmesh_neighbor_context};
    validate_geodata_job(candidate);
    for (const auto &map : maps)
      if (!std::filesystem::is_regular_file(client / "Maps" / (map + ".unr")))
        throw std::invalid_argument("Map input is missing: " + map);
    candidate.directory = territory::create_job_directory(m_output, client);
    candidate.id = territory::path_utf8(candidate.directory.filename());
    m_job =
        geodata_job_from_json(geodata_job_json(candidate), candidate.directory);
    territory::write_json_atomic(m_job.directory / "request.json",
                                 geodata_job_json(m_job), false);
    m_status = territory::Status{};
    m_status->job_id = m_job.id;
    m_status->map_count = m_job.maps.size();
    territory::write_json_atomic(m_job.directory / "status.json",
                                 territory::to_json(*m_status), false);
    m_process->start(m_executable, m_job.directory);
    m_running = true;
    return true;
  } catch (const std::exception &error) {
    fail(error.what());
    return false;
  }
}

void GeodataBuildController::poll() {
  if (!m_running)
    return;
  using territory::Phase;
  try {
    const auto code = m_process->exit_code();
    if (!m_failed && !m_forced) {
      const auto json = territory::read_json(m_job.directory / "status.json");
      auto status = territory::status_from_json(json, m_job.id);
      const auto format = json.value("format", std::string{});
      if ((!format.empty() && format != "l2j" && format != "navmesh") ||
          (format == "l2j" && !m_job.l2j) ||
          (format == "navmesh" && !m_job.navmesh))
        throw std::runtime_error("Unexpected geodata output format in status");
      m_format = format;
      if (status.map_count != m_job.maps.size() || !status.files.empty() ||
          (!status.map.empty() &&
           std::find(m_job.maps.begin(), m_job.maps.end(), status.map) ==
               m_job.maps.end()))
        throw std::runtime_error(
            "Geodata worker status does not match its request");
      m_status = std::move(status);
    }
    if (code) {
      m_running = false;
      if (m_failed)
        return;
      if (m_forced) {
        m_status->phase = Phase::Cancelled;
        m_status->error = "Cancelled (worker stopped after timeout)";
        return;
      }
      if (m_status->phase == Phase::Completed && *code == 0) {
        if (m_status->map_index != m_job.maps.size())
          throw std::runtime_error(
              "Geodata worker did not finish every requested map");
        for (const auto &name : geodata_output_names(m_job)) {
          const auto file = m_job.directory / name;
          if (!std::filesystem::is_regular_file(file) ||
              std::filesystem::file_size(file) == 0 ||
              !territory::is_within(file, m_job.directory))
            throw std::runtime_error("Missing geodata output: " + name);
        }
        if (!std::filesystem::is_regular_file(m_job.directory / "report.json"))
          throw std::runtime_error("Missing geodata build report");
        if (m_job.navmesh && m_job.navmesh_neighbor_context)
          for (const auto& map : m_job.maps) {
            const auto metadata=navmesh::read_region_metadata(m_job.directory/(map+".navmesh"));
            if (metadata.map!=map || navmesh::region_profile(metadata.settings)!=navmesh::region_profile(m_job.navmesh_settings))
              throw std::runtime_error("Navmesh metadata does not match job");
          }
      } else if (m_status->phase == Phase::Failed && *code != 0) {
        m_error = m_status->error;
      } else if (!(m_status->phase == Phase::Cancelled && *code == 130)) {
        fail("Geodata worker exited with code " + std::to_string(*code) +
             " without a matching result; see worker.log");
      }
      return;
    }
    if (!m_failed && m_status &&
        (m_status->phase == Phase::Completed ||
         m_status->phase == Phase::Cancelled ||
         m_status->phase == Phase::Failed))
      m_status->phase = Phase::Saving;
    if (m_cancel_deadline &&
        std::chrono::steady_clock::now() >= *m_cancel_deadline && !m_forced) {
      m_process->terminate_owned();
      m_forced = true;
    }
  } catch (const std::exception &error) {
    fail(error.what());
    if (m_running) {
      try {
        m_process->terminate_owned();
        if (m_process->exit_code())
          m_running = false;
      } catch (const std::exception &stop) {
        m_error += "; " + std::string(stop.what());
      }
    }
  }
}

void GeodataBuildController::cancel() {
  if (!m_running || m_cancel_deadline)
    return;
  try {
    territory::request_cancel(m_job.directory);
    m_cancel_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
  } catch (const std::exception &error) {
    fail(error.what());
    try {
      m_process->terminate_owned();
    } catch (const std::exception &stop) {
      m_error += "; " + std::string(stop.what());
    }
  }
}

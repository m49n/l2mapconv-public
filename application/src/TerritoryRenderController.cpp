#include "TerritoryRenderController.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <set>
#include <territory/PathIO.h>
namespace {
using territory::Phase;
bool success(Phase phase) {
  return phase == Phase::Completed || phase == Phase::CompletedWithWarnings;
}
bool terminal(Phase phase) {
  return success(phase) || phase == Phase::Cancelled || phase == Phase::Failed;
}
// Bounded UI check only. The worker checks every PNG chunk CRC before
// publishing. Do not decode/re-hash a 16K image on the viewer's frame thread.
bool png_metadata(const std::filesystem::path &file, int pixels) {
  std::ifstream in(file, std::ios::binary);
  std::array<unsigned char, 33> head{};
  std::array<unsigned char, 12> tail{};
  if (!in.read(reinterpret_cast<char *>(head.data()), head.size()))
    return false;
  constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71,
                                                   13,  10, 26, 10};
  auto number = [&](int i) {
    return (std::uint32_t(head[i]) << 24) | (std::uint32_t(head[i + 1]) << 16) |
           (std::uint32_t(head[i + 2]) << 8) | head[i + 3];
  };
  in.seekg(-12, std::ios::end);
  if (!in.read(reinterpret_cast<char *>(tail.data()), tail.size()))
    return false;
  return std::equal(signature.begin(), signature.end(), head.begin()) &&
         number(8) == 13 &&
         std::string(reinterpret_cast<char *>(head.data() + 12), 4) == "IHDR" &&
         number(16) == std::uint32_t(pixels) &&
         number(20) == std::uint32_t(pixels) && head[24] == 8 &&
         head[25] == 2 && head[26] == 0 && head[27] == 0 && head[28] == 0 &&
         tail == std::array<unsigned char, 12>{0,  0,  0,   0,  73, 69,
                                               78, 68, 174, 66, 96, 130};
}
} // namespace
struct TerritoryRenderController::Impl {
  std::filesystem::path exe, directory;
  std::unique_ptr<TerritoryProcess> process;
  Clock clock;
  std::optional<territory::Status> status;
  territory::Job job;
  std::string error;
  bool running{}, forced{}, broken{};
  std::optional<std::chrono::steady_clock::time_point> deadline;
  void fail(const std::string &message) {
    error = message;
    if (status) {
      status->phase = Phase::Failed;
      status->error = message;
    }
    broken = true;
  }
  void validate_status(const territory::Status &s, bool outputs) {
    if (s.map_count != job.maps.size() ||
        (!s.map.empty() &&
         std::find(job.maps.begin(), job.maps.end(), s.map) == job.maps.end()))
      throw std::runtime_error("Worker status does not match requested maps");
    std::set<std::string> allowed{"report.json"}, seen;
    for (auto &map : job.maps) {
      allowed.insert(map + "-report.json");
      if (job.mode == territory::Mode::Render)
        allowed.insert(map + "_" + std::to_string(job.settings.resolution) +
                       ".png");
    }
    for (auto &name : s.files) {
      if (!allowed.contains(name) || !seen.insert(name).second)
        throw std::runtime_error("Unexpected or duplicate worker output: " +
                                 name);
      if (outputs) {
        const auto path = directory / name;
        if (!std::filesystem::is_regular_file(path) ||
            std::filesystem::file_size(path) == 0 ||
            !territory::is_within(path, directory))
          throw std::runtime_error("Missing worker output: " + name);
        if (path.extension() == ".png" &&
            !png_metadata(path, job.settings.resolution))
          throw std::runtime_error("Invalid PNG dimensions: " + name);
      }
    }
    if (outputs && success(s.phase) &&
        (s.map_index != job.maps.size() || seen != allowed))
      throw std::runtime_error(
          "Worker completed without all requested outputs");
  }
};
TerritoryRenderController::TerritoryRenderController(
    std::filesystem::path exe, std::unique_ptr<TerritoryProcess> process,
    Clock clock)
    : impl(std::make_unique<Impl>()) {
  if (!process || !clock)
    throw std::invalid_argument("Controller requires a process and clock");
  impl->exe = std::move(exe);
  impl->process = std::move(process);
  impl->clock = std::move(clock);
}
TerritoryRenderController::~TerritoryRenderController() = default;
bool TerritoryRenderController::start(const std::filesystem::path &client,
                                      const std::vector<std::string> &maps,
                                      const territory::Settings &settings,
                                      const std::filesystem::path &output,
                                      territory::Mode mode) {
  auto &i = *impl;
  if (i.running)
    return false;
  i.error.clear();
  i.directory.clear();
  i.status.reset();
  i.deadline.reset();
  i.forced = false;
  i.broken = false;
  try {
    // Validate selection before creating anything. The eventual unique
    // directory is validated again.
    territory::Job candidate{
        "pending", std::filesystem::absolute(output) / "pending",
        client,    maps,
        mode,      settings};
    territory::validate_job(candidate);
    i.directory = territory::create_job_directory(output, client);
    candidate.directory = i.directory;
    candidate.id = territory::path_utf8(i.directory.filename());
    i.job =
        territory::job_from_json(territory::to_json(candidate), i.directory);
    territory::write_json_atomic(i.directory / "request.json",
                                 territory::to_json(i.job), false);
    i.status = territory::Status{};
    i.status->job_id = i.job.id;
    i.status->map_count = i.job.maps.size();
    territory::write_json_atomic(i.directory / "status.json",
                                 territory::to_json(*i.status), false);
    i.process->start(i.exe, i.directory);
    i.running = true;
    return true;
  } catch (const std::exception &e) {
    i.fail(e.what());
    return false;
  }
}
void TerritoryRenderController::poll() {
  auto &i = *impl;
  if (!i.running)
    return;
  try {
    auto code = i.process->exit_code();
    if (!i.broken && !i.forced) {
      auto candidate = territory::status_from_json(
          territory::read_json(i.directory / "status.json"), i.job.id);
      i.validate_status(candidate, code.has_value());
      i.status = std::move(candidate);
    }
    if (code) {
      i.running = false;
      if (i.broken)
        return;
      if (i.forced) {
        i.status->phase = Phase::Cancelled;
        i.status->error = "Cancelled (worker terminated after timeout)";
        return;
      }
      if (!terminal(i.status->phase) ||
          (success(i.status->phase) && *code != 0) ||
          (i.status->phase == Phase::Cancelled && *code != 130) ||
          (i.status->phase == Phase::Failed && *code == 0))
        i.fail("Worker exited with code " + std::to_string(*code) +
               " without a matching terminal result; see worker.log");
      else if (i.status->phase == Phase::Failed)
        i.error = i.status->error;
      return;
    }
    if (!i.broken && i.status && terminal(i.status->phase))
      i.status->phase = Phase::Saving;
    if (i.deadline && i.clock() >= *i.deadline && !i.forced) {
      i.process->terminate_owned();
      i.forced = true;
      if (i.process->exit_code()) {
        i.running = false;
        i.status->phase = Phase::Cancelled;
        i.status->error = "Cancelled (worker terminated after timeout)";
      }
    }
  } catch (const std::exception &e) {
    i.fail(e.what());
    try {
      i.process->terminate_owned();
      if (i.process->exit_code())
        i.running = false;
    } catch (const std::exception &stop) {
      i.error += "; " + std::string(stop.what());
    }
  }
}
void TerritoryRenderController::cancel() {
  auto &i = *impl;
  if (!i.running || i.deadline)
    return;
  try {
    territory::request_cancel(i.directory);
    i.deadline = i.clock() + std::chrono::seconds(5);
  } catch (const std::exception &e) {
    i.fail(e.what());
    i.process->terminate_owned();
  }
}
bool TerritoryRenderController::active() const { return impl->running; }
const std::optional<territory::Status> &
TerritoryRenderController::status() const {
  return impl->status;
}
const std::filesystem::path &TerritoryRenderController::job_directory() const {
  return impl->directory;
}
const std::string &TerritoryRenderController::error() const {
  return impl->error;
}

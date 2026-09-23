#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <streambuf>
#include <territory/FileIdentity.h>
#include <territory/JobRunner.h>
#include <territory/PathIO.h>
#include <territory/PngOutput.h>
#include <territory/VisualSceneLoader.h>
#include <utils/Log.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace territory {
namespace {
bool complete(Phase phase) {
  return phase == Phase::Completed || phase == Phase::CompletedWithWarnings;
}
struct SceneScope {
  std::unique_ptr<VisualScene> scene;
  std::function<void()> released;
  ~SceneScope() {
    scene.reset();
    if (released)
      released();
  }
};
#ifdef _WIN32
std::atomic_bool interrupted{};
BOOL WINAPI interrupt_handler(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
    interrupted = true;
    return TRUE;
  }
  return FALSE;
}
class WorkerLog : public std::streambuf {
public:
  explicit WorkerLog(const std::filesystem::path &path) {
    file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                       CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
      throw std::system_error(GetLastError(), std::system_category(),
                              "create worker log");
    old_cout = std::cout.rdbuf(this);
    old_cerr = std::cerr.rdbuf(this);
    old_color = utils::Log::colored;
    utils::Log::colored = false;
  }
  ~WorkerLog() {
    std::cout.flush();
    std::cerr.flush();
    std::cout.rdbuf(old_cout);
    std::cerr.rdbuf(old_cerr);
    utils::Log::colored = old_color;
    CloseHandle(file);
  }

protected:
  std::streamsize xsputn(const char *data, std::streamsize size) override {
    std::streamsize total = 0;
    while (total < size) {
      DWORD written = 0;
      auto n =
          static_cast<DWORD>(std::min<std::streamsize>(size - total, 65536));
      if (!WriteFile(file, data + total, n, &written, nullptr) || !written)
        break;
      total += written;
    }
    return total;
  }
  int_type overflow(int_type value) override {
    if (traits_type::eq_int_type(value, traits_type::eof()))
      return traits_type::not_eof(value);
    char c = traits_type::to_char_type(value);
    return xsputn(&c, 1) == 1 ? value : traits_type::eof();
  }
  int sync() override { return FlushFileBuffers(file) ? 0 : -1; }

private:
  HANDLE file{INVALID_HANDLE_VALUE};
  std::streambuf *old_cout{}, *old_cerr{};
  bool old_color{};
};
#endif
} // namespace
RunnerServices default_runner_services() {
  RunnerServices services;
  services.load = [](const std::filesystem::path &client,
                     const std::string &map, const Cancel &cancel) {
    return VisualSceneLoader(client).load(map, cancel);
  };
  services.render = [](const VisualScene &scene, const RasterSettings &settings,
                       const Cancel &cancel, const TileProgress &progress,
                       const Rows &rows) {
    check_cancel(cancel);
    TerritoryRenderer renderer;
    return renderer.render(scene, settings, cancel, progress, rows);
  };
  return services;
}
RunResult run_job(const Job &input, const Progress &progress,
                  const Cancel &external_cancel,
                  const RunnerServices &services) {
  Job job = input;
  RunResult result;
  result.status.job_id = job.id;
  result.status.map_count = job.maps.size();
  result.exit_code = 3;
  bool claimed = false;
  Report report;
  std::vector<std::string> processed;
  const auto started = std::chrono::steady_clock::now();
  auto cancel = [&] {
    return (external_cancel && external_cancel()) ||
           cancellation_requested(job.directory);
  };
  auto publish = [&] {
    write_json_atomic(job.directory / "status.json", to_json(result.status),
                      true);
    if (progress)
      progress(result.status);
  };
  auto report_json = [&] {
    auto json = to_json(report, job.id);
    std::vector<std::string> pending;
    for (auto &map : job.maps)
      if (std::find(processed.begin(), processed.end(), map) == processed.end())
        pending.push_back(map);
    json["request"] = to_json(job);
    json["state"] = phase_name(result.status.phase);
    json["error"] = result.status.error;
    json["processed_maps"] = processed;
    json["not_processed_maps"] = pending;
    json["elapsed_seconds"] = std::chrono::duration<double>(
                                  std::chrono::steady_clock::now() - started)
                                  .count();
    json["policy"] = {
        {"projection", "orthographic, north=-Y"},
        {"color", "RGB8 sRGB; color sampling and blending in linear light; "
                  "data masks linear"},
        {"lighting", "fixed neutral directional; no retail lightmaps/fog; "
                     "unlit where supported"},
        {"time_seconds", 0},
        {"transparency", "orthographic back-to-front draw bounds; intersecting "
                         "transparent geometry is approximate"},
        {"asset_parity",
         "Only explicitly reported material features are implemented; no claim "
         "that all retail assets or effects were reproduced"}};
    Json counts = Json::object();
    for (auto &issue : json["issues"]) {
      auto kind = issue["kind"].get<std::string>();
      counts[kind] = counts.value(kind, 0) + 1;
    }
    json["issue_counts"] = counts;
    return json;
  };
  try {
    validate_job(job);
    if (!services.load || (job.mode == Mode::Render && !services.render))
      throw std::invalid_argument("Missing runner services");
    std::sort(job.maps.begin(), job.maps.end());
    job.maps.erase(std::unique(job.maps.begin(), job.maps.end()),
                   job.maps.end());
    result.status.map_count = job.maps.size();
    if (!std::filesystem::is_directory(job.directory))
      throw std::invalid_argument("Job directory does not exist");
    if (std::filesystem::exists(job.directory / "request.json")) {
      if (to_json(job_from_json(read_json(job.directory / "request.json"),
                                job.directory)) != to_json(job))
        throw std::invalid_argument("Job differs from immutable request");
    } else
      write_json_atomic(job.directory / "request.json", to_json(job), false);
    // Exclusive publication is the run-once claim. A duplicate caller must not
    // change existing status.
    write_json_atomic(job.directory / "started.json",
                      {{"schema_version", 1}, {"job_id", job.id}}, false);
    claimed = true;
    result.status.phase = Phase::Preparing;
    publish();
    for (const auto &map : job.maps) {
      check_cancel(cancel);
      result.status.map = map;
      result.status.map_index = processed.size();
      result.status.tiles_done = 0;
      result.status.tiles_total = 0;
      result.status.phase = Phase::Loading;
      publish();
      const auto primary = job.client / "Maps" / (map + ".unr");
      const auto before = file_identity(primary);
      check_cancel(cancel);
      SceneScope owned{
          std::make_unique<VisualScene>(services.load(job.client, map, cancel)),
          services.scene_released};
      auto &scene = *owned.scene;
      validate_scene(scene);
      check_cancel(cancel);
      const auto after = file_identity(primary);
      if (before.sha256 != after.sha256 || before.bytes != after.bytes)
        throw std::runtime_error("Primary map changed during scene loading");
      report.issues.insert(report.issues.end(), scene.report.issues.begin(),
                           scene.report.issues.end());
      result.status.warnings = report.issues.size();
      Json metadata = scene.report.maps.empty() ? Json::object()
                                                : scene.report.maps.front();
      metadata["map"] = map;
      metadata["primary_input"] = {{"path", path_utf8(after.path)},
                                   {"bytes", after.bytes},
                                   {"sha256", after.sha256}};
      metadata["bounds"] = {
          {"min_x", scene.bounds.min_x}, {"min_y", scene.bounds.min_y},
          {"max_x", scene.bounds.max_x}, {"max_y", scene.bounds.max_y},
          {"min_z", scene.bounds.min_z}, {"max_z", scene.bounds.max_z}};
      metadata["resolution"] = job.mode == Mode::Render
                                   ? Json(job.settings.resolution)
                                   : Json(nullptr);
      metadata["world_units_per_pixel"] =
          job.mode == Mode::Render
              ? Json((scene.bounds.max_x - scene.bounds.min_x) /
                     job.settings.resolution)
              : Json(nullptr);
      metadata["water_enabled"] = job.settings.water;
      metadata["loaded_textures"] = scene.library.textures.size();
      metadata["material_count"] = scene.library.materials.size();
      metadata["material_inventory"] = material_inventory(scene);
      metadata["completed"] = false;
      const auto report_index = report.maps.size();
      report.maps.push_back(std::move(metadata));
      if (job.mode == Mode::Render) {
        const auto filename =
            map + "_" + std::to_string(job.settings.resolution) + ".png";
        PngOutput png(job.directory / filename, job.settings.resolution,
                      job.settings.resolution);
        result.status.phase = Phase::Rendering;
        publish();
        RasterSettings settings{job.settings.resolution, 2048, 4,
                                job.settings.water};
        auto info = services.render(
            scene, settings, cancel,
            [&](std::size_t done, std::size_t total) {
              if (done > total || done < result.status.tiles_done)
                throw std::runtime_error("Invalid raster progress");
              result.status.phase = Phase::Rendering;
              result.status.tiles_done = done;
              result.status.tiles_total = total;
              publish();
            },
            [&](int y, int width, int count,
                std::span<const std::uint8_t> bytes) {
              check_cancel(cancel);
              result.status.phase = Phase::Saving;
              publish();
              png.rows(y, width, count, bytes);
            });
        scene.report.issues.insert(scene.report.issues.end(),
                                   info.issues.begin(), info.issues.end());
        report.issues.insert(report.issues.end(), info.issues.begin(),
                             info.issues.end());
        result.status.warnings = report.issues.size();
        if (!info.issues.empty())
          report.maps[report_index]["material_inventory"] =
              material_inventory(scene);
        check_cancel(cancel);
        result.status.phase = Phase::Saving;
        publish();
        png.finish(cancel);
        if (!png_has_dimensions(job.directory / filename, settings.pixels,
                                settings.pixels))
          throw std::runtime_error("Published PNG is incomplete");
        result.status.files.push_back(filename);
        report.maps[report_index]["image"] = filename;
        report.maps[report_index]["gpu"] = info.gpu;
        report.maps[report_index]["msaa_samples"] = info.samples;
      }
      processed.push_back(map);
      report.maps[report_index]["completed"] = true;
      result.status.map_index = processed.size();
      // Immutable per-map checkpoints preserve completed work even if the next
      // map or process fails.
      const auto map_report = map + "-report.json";
      Report map_checkpoint;
      map_checkpoint.maps.push_back(report.maps[report_index]);
      map_checkpoint.issues = scene.report.issues;
      auto checkpoint = to_json(map_checkpoint, job.id);
      checkpoint["state"] =
          scene.report.issues.empty() ? "completed" : "completed_with_warnings";
      write_json_atomic(job.directory / map_report, checkpoint, false);
      result.status.files.push_back(map_report);
      publish();
    }
    check_cancel(cancel);
    result.status.phase = result.status.warnings ? Phase::CompletedWithWarnings
                                                 : Phase::Completed;
    result.exit_code = 0;
  } catch (const Cancelled &) {
    result.status.phase = Phase::Cancelled;
    result.status.error = "Cancelled";
    result.exit_code = 130;
  } catch (const std::exception &e) {
    result.status.phase = Phase::Failed;
    result.status.error = e.what();
    result.exit_code = claimed ? 3 : 2;
  }
  if (claimed) {
    try {
      const auto destination = job.directory / "report.json";
      write_json_atomic(destination, report_json(), false);
      result.report = destination;
      result.status.files.push_back("report.json");
      publish();
    } catch (const std::exception &e) {
      if (complete(result.status.phase)) {
        result.status.phase = Phase::Failed;
        result.exit_code = 3;
      }
      if (!result.status.error.empty())
        result.status.error += "; ";
      result.status.error +=
          "Unable to publish final report/status: " + std::string(e.what());
      try {
        publish();
      } catch (...) {
        std::cerr << result.status.error << '\n';
      }
    }
  }
  return result;
}
int run_job_file(const std::filesystem::path &directory) {
  try {
    if (!directory.is_absolute())
      throw std::invalid_argument("Internal job directory must be absolute");
    const auto job =
        job_from_json(read_json(directory / "request.json"), directory);
#ifdef _WIN32
    WorkerLog log(directory / "worker.log");
    interrupted = false;
    SetConsoleCtrlHandler(interrupt_handler, TRUE);
    struct HandlerScope {
      ~HandlerScope() { SetConsoleCtrlHandler(interrupt_handler, FALSE); }
    } handler;
    auto result = run_job(
        job, {}, [] { return interrupted.load(); }, default_runner_services());
    std::cout << result_json(job, result).dump() << '\n';
    return result.exit_code;
#else
    (void)job;
    throw std::runtime_error("Territory worker requires Windows");
#endif
  } catch (const std::exception &e) {
    std::cerr << "Invalid territory job: " << e.what() << '\n';
    return 2;
  }
}
Json result_json(const Job &job, const RunResult &result) {
  auto json = to_json(result.status);
  json["exit_code"] = result.exit_code;
  json["output_directory"] = path_utf8(job.directory);
  json["report"] =
      result.report.empty() ? Json(nullptr) : Json(path_utf8(result.report));
  return json;
}
} // namespace territory

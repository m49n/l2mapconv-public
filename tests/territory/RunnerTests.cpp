#include "Fixtures.h"
#include "TestSupport.h"
#include <fstream>
#include <territory/JobRunner.h>
#include <territory/PathIO.h>
#include <territory/PngOutput.h>
int runner_tests() {
  using namespace territory;
  int failures = 0;
  TestDirectory temp;
  auto client = temp.path() / "client";
  std::filesystem::create_directories(client / "Maps");
  for (auto map : {"22_22", "23_22"}) {
    std::ofstream file(client / "Maps" / (std::string(map) + ".unr"));
    file << "abc";
  }
  auto job = [&](std::string id, Mode mode) {
    return Job{id,     create_job_directory(temp.path() / "output", client),
               client, {"22_22", "23_22"},
               mode,   {4096, true}};
  };
  bool gpu_called = false;
  RunnerServices services;
  services.load = [](const auto &, const auto &, const Cancel &) {
    return make_visual_fixture();
  };
  services.render = [&](const auto &, const auto &, const auto &, const auto &,
                        const auto &) -> RasterInfo {
    gpu_called = true;
    throw std::runtime_error("Unexpected GPU");
  };
  auto inspect = job("inspect", Mode::Inspect);
  auto result = run_job(inspect, {}, [] { return false; }, services);
  failures += expect(result.exit_code == 0 && !gpu_called &&
                         result.status.phase == Phase::Completed,
                     "inspect never initializes GPU");
  failures += expect(std::filesystem::is_regular_file(result.report),
                     "inspect publishes report");
  const auto inspected = read_json(result.report);
  failures += expect(inspected["maps"][0].contains("material_inventory"),
                     "public inspect includes successful material inventory");
  failures +=
      expect(!std::filesystem::exists(inspect.directory / "22_22_4096.png"),
             "inspect creates no images");
  services.load = [](const auto &, const auto &, const Cancel &) {
    auto scene = make_visual_fixture();
    scene.report.issues.push_back(
        {IssueKind::MissingObject, "absent", "fixture", {}});
    return scene;
  };
  auto warning = job("warnings", Mode::Inspect);
  result = run_job(warning, {}, {}, services);
  failures += expect(result.exit_code == 0 &&
                         result.status.phase == Phase::CompletedWithWarnings &&
                         result.status.warnings == 2,
                     "missing-only warnings are not fatal");
  const auto first_checkpoint =
      read_json(warning.directory / "22_22-report.json");
  const auto second_checkpoint =
      read_json(warning.directory / "23_22-report.json");
  failures +=
      expect(first_checkpoint["maps"].size() == 1 &&
                 second_checkpoint["maps"].size() == 1 &&
                 second_checkpoint["maps"][0]["map"] == "23_22" &&
                 second_checkpoint["issues"].size() == 1,
             "per-map checkpoints contain only their own map and issues");
  failures += expect(second_checkpoint.dump().size() <=
                         first_checkpoint.dump().size() + 64,
                     "checkpoint size does not grow with earlier maps");
  auto cancelled = job("cancel", Mode::Inspect);
  result = run_job(cancelled, {}, [] { return true; }, services);
  failures +=
      expect(result.exit_code == 130 && result.status.phase == Phase::Cancelled,
             "cancellation has distinct exit code");
  int loaded = 0, released = 0;
  services.load = [&](const auto &, const auto &, const Cancel &) {
    if (loaded != released)
      throw std::runtime_error("Previous scene still alive");
    ++loaded;
    return make_visual_fixture();
  };
  services.scene_released = [&] { ++released; };
  services.render = [](const auto &, const RasterSettings &settings,
                       const Cancel &, const TileProgress &progress,
                       const Rows &rows) {
    std::vector<std::uint8_t> band(
        static_cast<std::size_t>(settings.pixels) * 128 * 3, 127);
    for (int y = 0; y < settings.pixels; y += 128)
      rows(y, settings.pixels, 128, band);
    if (progress)
      progress(4, 4);
    return RasterInfo{"fake CPU renderer", 4};
  };
  auto render = job("render", Mode::Render);
  result = run_job(render, {}, {}, services);
  failures += expect(
      result.exit_code == 0 && loaded == 2 && released == 2 &&
          png_has_dimensions(render.directory / "23_22_4096.png", 4096, 4096),
      "two maps release their scenes before next load");
  const auto ordinary_render = services.render;
  services.render = [&](const auto &scene, const auto &settings,
                        const auto &cancel, const auto &progress,
                        const auto &rows) {
    auto info = ordinary_render(scene, settings, cancel, progress, rows);
    info.issues.push_back(
        {IssueKind::Unsupported, "fixture", "GPU sampler limit", {"fixture"}});
    return info;
  };
  auto gpu_warning = job("gpu-warning", Mode::Render);
  auto gpu_result = run_job(gpu_warning, {}, {}, services);
  failures += expect(
      gpu_result.status.phase == Phase::CompletedWithWarnings &&
          gpu_result.status.warnings == 2 &&
          read_json(gpu_warning.directory / "22_22-report.json")["issues"]
                  .size() == 1,
      "GPU fallback warnings reach terminal status and each map report");
  services.render = ordinary_render;
  services.load = [&](const auto &, const std::string &map, const Cancel &) {
    if (map == "23_22")
      throw std::runtime_error("second map fails");
    return make_visual_fixture();
  };
  auto partial = job("partial", Mode::Render);
  result = run_job(partial, {}, {}, services);
  failures +=
      expect(result.exit_code == 3 && result.status.phase == Phase::Failed &&
                 png_has_dimensions(partial.directory / "22_22_4096.png", 4096,
                                    4096) &&
                 !std::filesystem::exists(partial.directory / "23_22_4096.png"),
             "failed second map preserves completed first image");
  auto again = run_job(render, {}, {}, services);
  failures += expect(again.exit_code != 0,
                     "completed job cannot be run twice in same directory");
  if (std::filesystem::exists(render.directory / "status.json"))
    failures += expect(
        status_from_json(read_json(render.directory / "status.json"), render.id)
                .phase == Phase::Completed,
        "duplicate worker does not corrupt completed status");
  return failures;
}

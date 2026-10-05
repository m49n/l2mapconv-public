#include "ExecutablePath.h"
#include "GeodataBuildController.h"
#include "MapSelectionContext.h"
#include "TerritoryRenderWindow.h"
#include "TestSupport.h"
#include "TestSuites.h"

#include <chrono>
#include <fstream>
#include <limits>
#include <territory/PathIO.h>
#include <thread>

namespace {
struct TestProcess : TerritoryProcess {
  std::optional<int> code;
  void start(const std::filesystem::path &,
             const std::filesystem::path &) override {}
  std::optional<int> exit_code() override { return code; }
  void terminate_owned() override { code = 130; }
};
struct Directory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      territory::path_from_utf8(
          "l2 geodata \xD0\xBA\xD0\xB0\xD1\x80\xD1\x82\xD0\xB0 test-" +
          std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count()));
  Directory() { std::filesystem::create_directories(path / "client" / "Maps"); }
  ~Directory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};
} // namespace

int run_geodata_build_tests() {
  int failures = run_navmesh_region_geometry_tests();
  Directory temp;
  const auto client = temp.path / "client";
  for (auto name : {"22_22", "24_18"})
    std::ofstream(client / "Maps" / (std::string(name) + ".unr"));
  MapSelectionContext selection{MapCatalog::discover(client)};
  selection.set_manual({22, 22}, true);
  selection.set_current({24, 18});
  const geodata::BuilderSettings chosen{64, 24, 40, 3, 20, 16, 2};
  {
    GeodataBuildJob job;
    job.id = "navmesh-format-test";
    job.directory = temp.path / "job";
    job.client = client;
    job.maps = {"22_22"};
    job.client_dat = false;
    job.l2j = false;
    job.navmesh = true;
    job.navmesh_neighbor_context = true;
    auto with_context = geodata_job_json(job);
    failures += expect(with_context.value("navmesh_neighbor_context", false), "new context mode is explicit in request");
    with_context.erase("navmesh_neighbor_context");
    failures += expect(!geodata_job_from_json(with_context, job.directory).navmesh_neighbor_context, "old request remains isolated");
    job.navmesh_neighbor_context = false;
    failures += expect(geodata_output_names(job) ==
                           std::vector<std::string>{"22_22.navmesh"},
                       "navmesh-only job requests no L2J or DAT");
    auto encoded = geodata_job_json(job);
    failures += expect(encoded.at("schema_version") == 2,
                       "new jobs use format-selection schema");
    try {
      const auto roundtrip = geodata_job_from_json(encoded, job.directory);
      failures +=
          expect(!roundtrip.l2j && roundtrip.navmesh && !roundtrip.client_dat,
                 "navmesh-only selection survives JSON round trip");
    } catch (const std::exception &e) {
      failures += expect(false, e.what());
    }
    job.l2j = true;
    job.client_dat = true;
    failures += expect(geodata_output_names(job) ==
                           std::vector<std::string>{
                               "22_22.l2j", "22_22_conv.dat", "22_22.navmesh"},
                       "both formats include all selected outputs");
    auto rejects_job = [&] {
      bool rejected = false;
      try {
        validate_geodata_job(job);
      } catch (const std::exception &) {
        rejected = true;
      }
      return rejected;
    };
    job.l2j = false;
    failures += expect(rejects_job(), "DAT without L2J is invalid");
    job.client_dat = false;
    job.navmesh = false;
    failures += expect(rejects_job(), "empty output selection is invalid");
    job.navmesh = true;
    job.settings.cell_size = 8;
    failures += expect(!rejects_job(),
                       "disabled L2J settings do not block navmesh-only jobs");
    job.settings.cell_size = 16;
    job.navmesh_settings.cell_size = 0;
    failures += expect(rejects_job(), "active navmesh settings are validated");
    job.navmesh = false;
    job.l2j = true;
    failures +=
        expect(!rejects_job(), "disabled navmesh settings do not block L2J");
    job.navmesh_settings.tile_cells = 512;
    try {
      const auto inactive =
          geodata_job_from_json(geodata_job_json(job), job.directory);
      failures +=
          expect(inactive.l2j && !inactive.navmesh,
                 "disabled invalid navmesh settings survive request creation");
    } catch (const std::exception &) {
      failures += expect(
          false,
          "disabled invalid tile_cells must not block L2J request creation");
    }
    auto legacy = geodata_job_json(job);
    legacy["schema_version"] = 1;
    legacy["client_dat"] = false;
    legacy.erase("outputs");
    legacy.erase("navmesh_settings");
    const auto old = geodata_job_from_json(legacy, job.directory);
    failures += expect(old.l2j && !old.navmesh && !old.client_dat,
                       "legacy jobs retain L2J semantics");
  }
  auto process = std::make_unique<TestProcess>();
  auto *child = process.get();
  GeodataBuildController controller{temp.path / "app.exe", temp.path / "output",
                                    std::move(process)};
  failures += expect(!controller.start(client, {}, chosen, true) &&
                         !std::filesystem::exists(temp.path / "output"),
                     "no selected map is rejected before creating output");
  auto invalid = chosen;
  invalid.cell_size = 8;
  failures += expect(!controller.start(client, {"22_22"}, invalid, true),
                     "unsafe client grid size is rejected");
  invalid = chosen;
  invalid.actor_height = std::numeric_limits<float>::quiet_NaN();
  failures += expect(!controller.start(client, {"22_22"}, invalid, true),
                     "nonfinite generator dimensions are rejected");
  failures += expect(
      controller.start(client, selected_render_maps(selection), chosen, false),
      "selected maps start a job");
  const auto request =
      territory::read_json(controller.job_directory() / "request.json");
  failures +=
      expect(request.at("maps") == territory::Json::array({"22_22"}) &&
                 request.at("settings").at("actor_height") == 64 &&
                 request.at("settings").at("actor_radius") == 24 &&
                 request.at("settings").at("max_walkable_angle") == 40 &&
                 request.at("settings").at("min_walkable_climb") == 3 &&
                 request.at("settings").at("max_walkable_climb") == 20 &&
                 request.at("settings").at("cell_size") == 16 &&
                 request.at("settings").at("cell_height") == 2 &&
                 request.at("outputs").at("client_dat") == false,
             "request captures chosen settings and excludes preview neighbors");
  failures += expect(!controller.start(client, {"24_18"}, chosen, true),
                     "running job cannot be replaced");
  auto status = *controller.status();
  status.phase = territory::Phase::Failed;
  status.error = "Map input is missing";
  territory::write_json_atomic(controller.job_directory() / "status.json",
                               territory::to_json(status), true);
  child->code = 3;
  controller.poll();
  failures += expect(!controller.active() &&
                         controller.error() == "Map input is missing",
                     "worker failure is displayed and releases running state");
  failures += expect(controller.start(client, {"24_18"}, chosen, true),
                     "new build uses a fresh output directory after failure");
  child->code.reset();
  status = *controller.status();
  status.phase = territory::Phase::Completed;
  status.map_index = 1;
  territory::write_json_atomic(controller.job_directory() / "status.json",
                               territory::to_json(status), true);
  child->code = 0;
  controller.poll();
  failures += expect(controller.status()->phase == territory::Phase::Failed,
                     "worker completion without requested files is rejected");
  failures += expect(controller.start(client, {"22_22"}, chosen, false),
                     "another isolated build can start");
  child->code.reset();
  controller.cancel();
  failures += expect(
      std::filesystem::exists(controller.job_directory() / "cancel.request"),
      "cancel signals only the current job");
  status = *controller.status();
  status.phase = territory::Phase::Cancelled;
  territory::write_json_atomic(controller.job_directory() / "status.json",
                               territory::to_json(status), true);
  child->code = 130;
  controller.poll();
  failures += expect(!controller.active() && controller.status()->phase ==
                                                 territory::Phase::Cancelled,
                     "cooperative cancellation releases the build state");

  for (bool output_exists : {false, true}) {
    child->code.reset();
    failures +=
        expect(controller.start(client, {"24_18"}, chosen, false, false, true),
               "navmesh-only controller starts independently");
    status = *controller.status();
    status.phase = territory::Phase::Completed;
    status.map_index = 1;
    auto json = territory::to_json(status);
    json["format"] = "navmesh";
    territory::write_json_atomic(controller.job_directory() / "status.json",
                                 json, true);
    territory::write_json_atomic(controller.job_directory() / "report.json", {},
                                 false);
    if (output_exists)
      std::ofstream(controller.job_directory() / "24_18.navmesh") << "fixture";
    child->code = 0;
    controller.poll();
    failures += expect(controller.status()->phase ==
                           (output_exists ? territory::Phase::Completed
                                          : territory::Phase::Failed),
                       "navmesh completion requires navmesh output, not L2J");
    failures += expect(controller.format() == "navmesh",
                       "format progress reaches the UI controller");
  }

#ifdef _WIN32
  // Exercise the real executable, request decoding and process mode without
  // expensive map generation. Removing this fixture map after recording the
  // request simulates an input disappearing before the child loads it.
  const auto executable =
      running_executable_path().parent_path() / "l2mapconv.exe";
  failures += expect(std::filesystem::is_regular_file(executable),
                     "real geodata worker executable is built alongside tests");
  if (std::filesystem::is_regular_file(executable)) {
    // A pre-cancelled navmesh-only job must not attempt to decode this empty
    // fixture map. Re-running it must preserve every original output/status.
    const auto cancelled_dir = temp.path / "cancelled-navmesh";
    std::filesystem::create_directory(cancelled_dir);
    GeodataBuildJob cancelled_job;
    cancelled_job.id = "cancelled-navmesh";
    cancelled_job.directory = cancelled_dir;
    cancelled_job.client = client;
    cancelled_job.maps = {"24_18"};
    cancelled_job.l2j = false;
    cancelled_job.client_dat = false;
    cancelled_job.navmesh = true;
    territory::write_json_atomic(cancelled_dir / "request.json",
                                 geodata_job_json(cancelled_job), false);
    territory::request_cancel(cancelled_dir);
    auto run_native = [&](const auto &directory) {
      auto native = make_territory_process(L"--geodata-job");
      native->start(executable, directory);
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(5);
      std::optional<int> code;
      do {
        code = native->exit_code();
        if (code)
          break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      } while (std::chrono::steady_clock::now() < deadline);
      return code;
    };
    failures += expect(run_native(cancelled_dir) == 130,
                       "native navmesh cancellation returns 130");
    const auto cancelled_status =
        territory::read_json(cancelled_dir / "status.json");
    const auto cancelled_report =
        territory::read_json(cancelled_dir / "report.json");
    failures +=
        expect(cancelled_status.at("state") == "cancelled" &&
                   cancelled_report.at("files").empty() &&
                   !std::filesystem::exists(cancelled_dir / "24_18.navmesh"),
               "cancelled job publishes no unfinished navmesh");
    failures +=
        expect(run_native(cancelled_dir) == 2 &&
                   territory::read_json(cancelled_dir / "status.json") ==
                       cancelled_status &&
                   territory::read_json(cancelled_dir / "report.json") ==
                       cancelled_report,
               "repeated worker invocation cannot overwrite a claimed job");
    failures += expect(controller.start(client, {"22_22"}, chosen, true),
                       "native failure fixture records a valid request");
    child->code.reset();
    std::filesystem::remove(client / "Maps" / "22_22.unr");
    auto native = make_territory_process(L"--geodata-job");
    native->start(executable, controller.job_directory());
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::optional<int> code;
    do {
      code = native->exit_code();
      if (code)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    failures +=
        expect(code == 3,
               "native geodata child reports missing input as build failure");
    if (code == 3) {
      const auto result = territory::status_from_json(
          territory::read_json(controller.job_directory() / "status.json"),
          controller.status()->job_id);
      failures += expect(result.phase == territory::Phase::Failed &&
                             result.error == "Map input is missing: 22_22" &&
                             std::filesystem::is_regular_file(
                                 controller.job_directory() / "worker.log"),
                         "native worker publishes diagnostic status and a log");
      const auto report =
          territory::read_json(controller.job_directory() / "report.json");
      failures += expect(report.at("settings").at("actor_height") == 64 &&
                             report.at("outputs").at("client_dat") == true &&
                             report.at("files").empty(),
                         "native worker preserves selected settings without "
                         "publishing geodata on failure");
    }
  }
#endif
  return failures;
}

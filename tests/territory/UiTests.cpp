#include "ClientSessionContext.h"
#include "Fixtures.h"
#include "MapSelectionContext.h"
#include "TerritoryCommandLine.h"
#include "TerritoryRenderController.h"
#include "TerritoryRenderWindow.h"
#include "TestSupport.h"
#include <fstream>
#include <territory/PathIO.h>
namespace {
class UiProcess : public TerritoryProcess {
  void start(const std::filesystem::path &,
             const std::filesystem::path &) override {}
  std::optional<int> exit_code() override { return std::nullopt; }
  void terminate_owned() override {}
};
} // namespace
int ui_tests() {
  using namespace territory;
  int failures = 0;
  TestDirectory temp;
  const auto root = temp.path() / "client";
  std::filesystem::create_directories(root / "Maps");
  for (auto map : {"22_22", "24_18"}) {
    std::ofstream file(root / "Maps" / (std::string(map) + ".unr"));
  }
  MapSelectionContext selection(MapCatalog::discover(root));
  failures += expect(!can_start_territory(selection, false),
                     "no selection disables render");
  selection.set_manual({22, 22}, true);
  selection.set_current({24, 18});
  failures += expect(selected_render_maps(selection) ==
                         std::vector<std::string>{"22_22"},
                     "only manual maps exported, not current/neighbors");
  failures += expect(can_start_territory(selection, false) &&
                         !can_start_territory(selection, true),
                     "busy state disables render");
  int browse_calls = 0;
  ClientSessionContext session(root, {}, [&] {
    ++browse_calls;
    return std::optional(root);
  });
  session.set_switch_blocked(true);
  failures += expect(session.switch_blocked() &&
                         !session.request_switch(root) && !session.browse() &&
                         !session.requested_client() && browse_calls == 0,
                     "busy client switching blocked before folder dialog");
  session.set_switch_blocked(false);
  failures +=
      expect(session.request_switch(root), "switch restored after completion");
  for (int resolution : {1024, 2048, 4096, 8192, 16384}) {
    auto cli = parse_territory_command(
        {"app", "--render-territory", "--client-root", path_utf8(root),
         "--output", path_utf8(temp.path() / "out"), "--resolution",
         std::to_string(resolution), "--no-water", "--", "22_22"});
    if (cli) {
      TerritoryRenderController controller(temp.path() / "app.exe",
                                           std::make_unique<UiProcess>());
      failures +=
          expect(controller.start(root, selected_render_maps(selection),
                                  {resolution, false}, temp.path() / "out"),
                 "UI starts matching job");
      if (controller.active()) {
        const auto job = job_from_json(
            read_json(controller.job_directory() / "request.json"),
            controller.job_directory());
        failures +=
            expect(job.client == cli->client && job.maps == cli->maps &&
                       job.settings.resolution == cli->settings.resolution &&
                       job.settings.water == cli->settings.water &&
                       job.mode == cli->mode,
                   "UI and CLI job settings agree");
      }
    }
  }
  {
    Settings chosen{4096, false, false, true, 120.0, 55.0};
    auto cli = parse_territory_command(
        {"app", "--render-territory", "--client-root", path_utf8(root),
         "--output", path_utf8(temp.path() / "sun-out"), "--resolution", "4096",
         "--no-water", "--no-textures", "--shadows", "--sun-azimuth", "120",
         "--sun-elevation", "55", "--", "22_22"});
    TerritoryRenderController controller(temp.path() / "app.exe",
                                         std::make_unique<UiProcess>());
    failures += expect(
        cli && controller.start(root, selected_render_maps(selection), chosen,
                                temp.path() / "sun-out"),
        "UI starts selected appearance job");
    if (controller.active() && cli) {
      const auto stored = job_from_json(
          read_json(controller.job_directory() / "request.json"),
          controller.job_directory());
      failures += expect(
          stored.settings.resolution == cli->settings.resolution &&
              stored.settings.water == cli->settings.water &&
              stored.settings.textures == cli->settings.textures &&
              stored.settings.shadows == cli->settings.shadows &&
              stored.settings.sun_azimuth_deg == cli->settings.sun_azimuth_deg &&
              stored.settings.sun_elevation_deg ==
                  cli->settings.sun_elevation_deg,
          "UI and CLI persist the same shadow appearance request");
    }
  }
  TerritoryRenderController inspect(temp.path() / "app.exe",
                                    std::make_unique<UiProcess>());
  failures += expect(inspect.start(root, {"22_22"}, {}, temp.path() / "inspect",
                                   Mode::Inspect),
                     "inspect starts from UI");
  if (inspect.active())
    failures += expect(
        job_from_json(read_json(inspect.job_directory() / "request.json"),
                      inspect.job_directory())
                .mode == Mode::Inspect,
        "UI inspection requests no raster");
  return failures;
}

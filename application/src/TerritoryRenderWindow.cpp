#include "TerritoryRenderWindow.h"
#include "ClientFolderPicker.h"
#include "ClientSessionContext.h"
#include "MapSelectionContext.h"
#include "TerritoryRenderController.h"
#include <imgui.h>
#include <territory/PathIO.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>
#endif
void draw_territory_render_window(TerritoryRenderViewState &view,
                                  TerritoryRenderController &controller,
                                  const MapSelectionContext &selection,
                                  ClientSessionContext &client) {
  ImGui::SetNextWindowPos({990, 540}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({430, 380}, ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Territory Render")) {
    ImGui::End();
    return;
  }
  const auto maps = selected_render_maps(selection);
  const bool busy = controller.active();
  ImGui::Text("Selected maps: %zu", maps.size());
  ImGui::TextWrapped("Uses checkboxes in Maps, not auto-loaded neighbors. "
                     "Preview detail settings do not affect export.");
  if (!maps.empty()) {
    ImGui::BeginChild("RenderSelection", {0, 50}, true);
    for (const auto &map : maps) {
      ImGui::TextUnformatted(map.c_str());
      ImGui::SameLine();
    }
    ImGui::EndChild();
  }
  ImGui::BeginDisabled(busy);
  int resolution = view.settings.resolution == 4096   ? 0
                   : view.settings.resolution == 8192 ? 1
                                                      : 2;
  ImGui::SetNextItemWidth(120);
  if (ImGui::Combo("Resolution", &resolution,
                   "4K (4096)\0 8K (8192)\0 16K (16384)\0"))
    view.settings.resolution = 4096 << resolution;
  ImGui::Checkbox("Water (supported surfaces)", &view.settings.water);
  ImGui::TextWrapped("Output: %s", territory::path_utf8(view.output).c_str());
  if (ImGui::Button("Browse output...")) {
    if (auto chosen = choose_directory(
            L"Choose a territory output directory outside the client"))
      view.output = *chosen;
  }
  ImGui::EndDisabled();
  ImGui::BeginDisabled(!can_start_territory(selection, busy) ||
                       client.requested_client().has_value());
  if (ImGui::Button("Render selected")) {
    view.error.clear();
    controller.start(client.current_client(), maps, view.settings, view.output);
    client.set_switch_blocked(controller.active());
  }
  ImGui::SameLine();
  if (ImGui::Button("Inspect textures")) {
    view.error.clear();
    controller.start(client.current_client(), maps, view.settings, view.output,
                     territory::Mode::Inspect);
    client.set_switch_blocked(controller.active());
  }
  ImGui::EndDisabled();
  if (controller.active()) {
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      controller.cancel();
  }
  if (const auto &status = controller.status()) {
    ImGui::Separator();
    ImGui::Text("State: %s",
                std::string(territory::phase_name(status->phase)).c_str());
    ImGui::Text("Map: %s | Finished: %zu/%zu", status->map.c_str(),
                status->map_index, status->map_count);
    ImGui::Text("Tiles: %zu/%zu | Issues: %zu", status->tiles_done,
                status->tiles_total, status->warnings);
    if (status->tiles_total)
      ImGui::ProgressBar(static_cast<float>(status->tiles_done) /
                         status->tiles_total);
    if (!status->error.empty())
      ImGui::TextWrapped("%s", status->error.c_str());
  }
  if (!controller.error().empty())
    ImGui::TextWrapped("Error: %s", controller.error().c_str());
  if (!controller.job_directory().empty()) {
    ImGui::TextWrapped(
        "Job / report folder: %s",
        territory::path_utf8(controller.job_directory()).c_str());
    if (ImGui::Button("Open output folder")) {
#ifdef _WIN32
      const auto result =
          ShellExecuteW(nullptr, L"open", controller.job_directory().c_str(),
                        nullptr, nullptr, SW_SHOWNORMAL);
      if (reinterpret_cast<std::intptr_t>(result) <= 32)
        view.error = "Unable to open output folder.";
#else
      view.error = "Opening folders requires Windows.";
#endif
    }
  }
  if (!view.error.empty())
    ImGui::TextWrapped("%s", view.error.c_str());
  ImGui::TextWrapped(
      "PNG is lossless RGB8 sRGB, 4x MSAA where supported. Missing/approximate "
      "materials are listed in report.json.");
  ImGui::End();
}

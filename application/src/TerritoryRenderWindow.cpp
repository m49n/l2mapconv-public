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
  ImGui::SetNextWindowSize({430, 500}, ImGuiCond_FirstUseEver);
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
  const auto resolution_label = [](int pixels) {
    return std::to_string(pixels / 1024) + "K (" + std::to_string(pixels) + ")";
  };
  ImGui::SetNextItemWidth(120);
  if (ImGui::BeginCombo("Resolution",
                        resolution_label(view.settings.resolution).c_str())) {
    for (int pixels : territory::render_resolutions) {
      const bool selected = pixels == view.settings.resolution;
      if (ImGui::Selectable(resolution_label(pixels).c_str(), selected))
        view.settings.resolution = pixels;
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::Checkbox("Water (supported surfaces)", &view.settings.water);
  ImGui::Checkbox("Textures", &view.settings.textures);
  ImGui::Separator();
  ImGui::TextUnformatted("Shadows");
  ImGui::Checkbox("Enable shadows", &view.settings.shadows);
  ImGui::BeginDisabled(!view.settings.shadows);
  const double azimuth_min = 0.0, azimuth_max = 360.0;
  const double elevation_min = 15.0, elevation_max = 80.0;
  ImGui::SliderScalar("Sun direction", ImGuiDataType_Double,
                      &view.settings.sun_azimuth_deg, &azimuth_min,
                      &azimuth_max, "%.0f deg");
  ImGui::SliderScalar("Sun elevation", ImGuiDataType_Double,
                      &view.settings.sun_elevation_deg, &elevation_min,
                      &elevation_max, "%.0f deg");
  ImGui::EndDisabled();
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
    controller.start(client.current_client(), maps, territory::Settings{}, view.output,
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

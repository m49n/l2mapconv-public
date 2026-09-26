#include "pch.h"

#include "UISettings.h"
#include "UISystem.h"
#include "TerritoryRenderController.h"

#include <string>

namespace {

auto map_state_label(const MapSelectionContext &selection,
                     MapCoordinate coordinate) -> const char * {
  const auto detail = selection.status(coordinate, MapLayer::Detail);
  const auto terrain = selection.status(coordinate, MapLayer::Terrain);
  if (detail == MapResidencyStatus::Failed ||
      terrain == MapResidencyStatus::Failed) {
    return "!";
  }
  if (detail == MapResidencyStatus::Loading ||
      terrain == MapResidencyStatus::Loading) {
    return "L";
  }
  if (detail == MapResidencyStatus::Queued ||
      terrain == MapResidencyStatus::Queued) {
    return "Q";
  }
  if (detail == MapResidencyStatus::Resident) {
    return "D";
  }
  if (terrain == MapResidencyStatus::Resident) {
    return "T";
  }
  return "-";
}

auto path_label(const std::filesystem::path &path) -> std::string {
  const auto value = path.u8string();
  return {value.begin(), value.end()};
}

} // namespace

UISystem::UISystem(UIContext &ui_context, WindowContext &window_context,
                   RenderingContext &rendering_context)
    : m_ui_context{ui_context}, m_window_context{window_context},
      m_rendering_context{rendering_context} {

  ASSERT(m_window_context.window_handle != nullptr, "App",
         "Window must be initialized");

  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = imgui_ini_filename();
  ImGui_ImplGlfw_InitForOpenGL(m_window_context.window_handle, true);
  ImGui_ImplOpenGL3_Init();

  ImGui::StyleColorsDark();

  // Default rendering settings
  m_ui_context.rendering.set_defaults();

  // Default geodata settings
  m_ui_context.geodata.set_defaults();
}

UISystem::UISystem(UIContext &ui_context, WindowContext &window_context,
                   RenderingContext &rendering_context,
                   MapSelectionContext &map_selection_context,
                   ClientSessionContext &client_session_context,
                   TerritoryRenderController *territory_controller,
                   TerritoryRenderViewState *territory_view)
    : UISystem{ui_context, window_context, rendering_context} {
  m_map_selection_context = &map_selection_context;
  m_client_session_context = &client_session_context;
  m_territory_controller = territory_controller;
  m_territory_view = territory_view;
}

UISystem::~UISystem() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void UISystem::frame_begin(Timestep frame_time) {
  if (m_territory_controller != nullptr) {
    m_territory_controller->poll();
    m_client_session_context->set_switch_blocked(m_territory_controller->active());
  }
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  if (m_map_selection_context != nullptr) {
    maps_window();
  }
  if (m_client_session_context != nullptr) {
    client_window();
  }
  rendering_window(frame_time);
  geodata_window();
  if (m_territory_controller != nullptr && m_territory_view != nullptr) {
    draw_territory_render_window(*m_territory_view, *m_territory_controller,
                                *m_map_selection_context, *m_client_session_context);
  }
}

void UISystem::client_window() const {
  auto &client = *m_client_session_context;
  const auto current = path_label(client.current_client());

  ImGui::Begin("Client", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::TextUnformatted("Current client:");
  ImGui::TextWrapped("%s", current.c_str());
  ImGui::BeginDisabled(client.switch_blocked());
  if (ImGui::Button("Browse...")) {
    client.browse();
  }

  if (!client.recent_clients().empty()) {
    ImGui::Separator();
    ImGui::TextUnformatted("Recent clients:");
    for (std::size_t index = 0; index < client.recent_clients().size();
         ++index) {
      const auto label = path_label(client.recent_clients()[index]);
      ImGui::PushID(static_cast<int>(index));
      if (ImGui::Button(label.c_str())) {
        client.request_switch(client.recent_clients()[index]);
      }
      ImGui::PopID();
    }
  }

  ImGui::EndDisabled();
  if (client.switch_blocked()) {
    ImGui::TextUnformatted("Client switching is locked while a territory job runs.");
  }
  if (!client.error().empty()) {
    ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "%s", client.error().c_str());
  }
  ImGui::End();
}

void UISystem::frame_end(Timestep /*frame_time*/) {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void UISystem::rendering_window(Timestep frame_time) const {
  if (m_window_context.keyboard.m) {
    m_ui_context.rendering.wireframe = !m_ui_context.rendering.wireframe;
  }

  const auto &camera_position = m_rendering_context.camera.position();

  ImGui::Begin("Rendering", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("CPU frame time: %f", frame_time.seconds());
  ImGui::Text("Draws: %d", m_ui_context.rendering.draws);
  ImGui::Text("Camera");
  ImGui::Text("\tx: %d", static_cast<int>(camera_position.x));
  ImGui::Text("\ty: %d", static_cast<int>(camera_position.y));
  ImGui::Text("\tz: %d", static_cast<int>(camera_position.z));
  if (m_map_selection_context != nullptr) {
    ImGui::Text("Current map: %s",
                m_map_selection_context->current_label().c_str());
    auto auto_load = m_map_selection_context->auto_load();
    if (ImGui::Checkbox("Auto-load current map", &auto_load)) {
      m_map_selection_context->set_auto_load(auto_load);
    }
    auto include_neighbors = m_map_selection_context->include_neighbors();
    ImGui::BeginDisabled(!auto_load);
    if (ImGui::Checkbox("Include +1 neighbors", &include_neighbors)) {
      m_map_selection_context->set_include_neighbors(include_neighbors);
    }
    ImGui::EndDisabled();
  }
  ImGui::InputFloat("Camera Speed", &m_ui_context.camera.speed, 100.0f, 1000.0f,
                    "%.0f");
  ImGui::InputFloat("Mouse Sensitivity", &m_ui_context.camera.mouse_sensitivity,
                    0.0001f, 0.001f, "%.4f");
  ImGui::TextUnformatted(
      "RMB look | WASD move | Space/Ctrl vertical | Shift fast | Alt slow | "
      "M wireframe");
  ImGui::Separator();
  ImGui::TextUnformatted("Live Scene (3D flight)");
  auto &live = m_ui_context.rendering.live;
  const auto &live_status = m_ui_context.rendering.live_diagnostics;
  ImGui::Checkbox("Water##live", &live.water);
  ImGui::Checkbox("Textures##live", &live.textures);
  ImGui::Checkbox("Shadows##live", &live.shadows);
  ImGui::BeginDisabled(!live.shadows);
  ImGui::SliderFloat("Sun direction##live", &live.sun_azimuth_deg, 0.f,
                     360.f, "%.0f deg");
  ImGui::SliderFloat("Sun elevation##live", &live.sun_elevation_deg, 15.f,
                     80.f, "%.0f deg");
  ImGui::EndDisabled();
  ImGui::Text("Live draws: %d | Shadow map: %d px",
              live_status.draws, live_status.shadow_map_size);
  if (live_status.omitted_casters)
    ImGui::Text("Unreliable shadow casters omitted: %d",
                live_status.omitted_casters);
  if (live_status.fallback_textures)
    ImGui::Text("Neutral texture fallbacks: %d",
                live_status.fallback_textures);
  if (live_status.fallback_materials)
    ImGui::Text("Neutral material fallbacks: %d",
                live_status.fallback_materials);
  if (!live_status.error.empty())
    ImGui::TextWrapped("Live scene error: %s", live_status.error.c_str());
  ImGui::TextWrapped("Water appears only where a supported water surface is "
                     "present; a WaterVolume alone is not drawable.");
  ImGui::Separator();
  ImGui::TextUnformatted("Geometry and overlays");
#ifdef LOAD_TEXTURES
  ImGui::Checkbox("Legacy textures", &m_ui_context.rendering.textures);
#endif
  ImGui::Checkbox("Culling", &m_ui_context.rendering.culling);
  ImGui::Checkbox("Wireframe", &m_ui_context.rendering.wireframe);
  ImGui::Checkbox("Passable", &m_ui_context.rendering.passable);
  ImGui::Checkbox("Terrain", &m_ui_context.rendering.terrain);
  ImGui::Checkbox("Static Meshes", &m_ui_context.rendering.static_meshes);
  ImGui::Checkbox("CSG", &m_ui_context.rendering.csg);
  ImGui::Checkbox("Blocking Volumes", &m_ui_context.rendering.blocking_volumes);
  ImGui::Checkbox("Bounding Boxes", &m_ui_context.rendering.bounding_boxes);
  ImGui::Checkbox("Imported Geodata", &m_ui_context.rendering.imported_geodata);
  ImGui::Checkbox("Generated Geodata",
                  &m_ui_context.rendering.generated_geodata);
  ImGui::End();
}

void UISystem::maps_window() const {
  auto &selection = *m_map_selection_context;
  const auto &catalog = selection.catalog();
  const auto summary = selection.summary();

  ImGui::Begin("Maps");
  if (ImGui::Button("Select All")) {
    selection.select_all_manual();
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Manual")) {
    selection.clear_manual();
  }

  ImGui::Text("Manual: %zu | Terrain: %zu | Detail: %zu",
              summary.manual_selected, summary.terrain_resident,
              summary.detail_resident);
  ImGui::Text("Queued: %zu | Loading: %zu | Failed: %zu", summary.queued,
              summary.loading, summary.failed);
  ImGui::TextUnformatted("State: - unloaded | Q queued | L loading | T terrain "
                         "| D detail | ! failed");

  if (!catalog.regions().empty()) {
    const auto extents = catalog.extents();
    const auto x_count = extents.max_x - extents.min_x + 1;
    const auto column_count = x_count + 1;
    constexpr auto column_width = 86.0f;
    const auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                       ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
                       ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("MapGrid", column_count, flags, {0.0f, 420.0f},
                          column_count * column_width)) {
      ImGui::TableSetupScrollFreeze(1, 1);
      ImGui::TableSetupColumn("Y \\ X", ImGuiTableColumnFlags_WidthFixed,
                              48.0f);
      for (auto x = extents.min_x; x <= extents.max_x; ++x) {
        const auto label = std::to_string(x);
        ImGui::TableSetupColumn(label.c_str(), ImGuiTableColumnFlags_WidthFixed,
                                column_width);
      }
      ImGui::TableHeadersRow();

      for (auto y = extents.min_y; y <= extents.max_y; ++y) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", y);
        for (auto x = extents.min_x; x <= extents.max_x; ++x) {
          ImGui::TableSetColumnIndex(x - extents.min_x + 1);
          const MapCoordinate coordinate{x, y};
          const auto *region = catalog.find(coordinate);
          if (region == nullptr) {
            continue;
          }

          auto selected = selection.manual_selection().contains(coordinate);
          const auto id =
              "##map_" + std::to_string(x) + "_" + std::to_string(y);
          if (ImGui::Checkbox(id.c_str(), &selected)) {
            selection.set_manual(coordinate, selected);
          }
          ImGui::SameLine();
          ImGui::Text("%s %s", region->name.c_str(),
                      map_state_label(selection, coordinate));
        }
      }
      ImGui::EndTable();
    }
  }
  ImGui::End();
}

void UISystem::geodata_window() const {
  ImGui::Begin("Geodata", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  if (m_ui_context.geodata.streaming_preview) {
    ImGui::TextWrapped(
        "Streamed preview geometry is not a geodata generation input. Use "
        "the CLI --build workflow for complete map geometry.");
  }

  ImGui::PushItemWidth(50);
  ImGui::InputFloat("Actor Height", &m_ui_context.geodata.actor_height);
  ImGui::InputFloat("Actor Radius", &m_ui_context.geodata.actor_radius);
  ImGui::InputFloat("Max Walkable Angle",
                    &m_ui_context.geodata.max_walkable_angle);
  ImGui::InputFloat("Min Walkable Climb",
                    &m_ui_context.geodata.min_walkable_climb);
  ImGui::InputFloat("Max Walkable Climb",
                    &m_ui_context.geodata.max_walkable_climb);
  ImGui::InputFloat("Cell Size", &m_ui_context.geodata.cell_size);
  ImGui::InputFloat("Cell Height", &m_ui_context.geodata.cell_height);

  ImGui::BeginDisabled(m_ui_context.geodata.streaming_preview);
  if (ImGui::Button("Reset")) {
    m_ui_context.geodata.set_defaults();

    ASSERT(m_ui_context.geodata.build_handler, "App",
           "Geodata build handler must be defined");
    m_ui_context.geodata.build_handler();
  }

  ImGui::SameLine();

  if (ImGui::Button("Build")) {
    ASSERT(m_ui_context.geodata.build_handler, "App",
           "Geodata build handler must be defined");
    m_ui_context.geodata.build_handler();
  }
  ImGui::EndDisabled();

  ImGui::SameLine();

  ImGui::Checkbox("Export", &m_ui_context.geodata.should_export);

  ImGui::End();
}

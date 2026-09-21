#include "pch.h"

#include "UISystem.h"

UISystem::UISystem(UIContext &ui_context, WindowContext &window_context,
                   RenderingContext &rendering_context)
    : m_ui_context{ui_context}, m_window_context{window_context},
      m_rendering_context{rendering_context} {

  ASSERT(m_window_context.window_handle != nullptr, "App",
         "Window must be initialized");

  IMGUI_CHECKVERSION();

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForOpenGL(m_window_context.window_handle, true);
  ImGui_ImplOpenGL3_Init();

  ImGui::StyleColorsDark();

  // Default rendering settings
  m_ui_context.rendering.set_defaults();

  // Default geodata settings
  m_ui_context.geodata.set_defaults();
}

UISystem::~UISystem() {
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void UISystem::frame_begin(Timestep frame_time) {
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  rendering_window(frame_time);
  geodata_window();
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
  ImGui::InputFloat("Camera Speed", &m_ui_context.camera.speed, 100.0f,
                    1000.0f, "%.0f");
  ImGui::InputFloat("Mouse Sensitivity",
                    &m_ui_context.camera.mouse_sensitivity, 0.0001f, 0.001f,
                    "%.4f");
  ImGui::TextUnformatted(
      "RMB look | WASD move | Space/Ctrl vertical | Shift fast | Alt slow | "
      "M wireframe");
#ifdef LOAD_TEXTURES
  ImGui::Checkbox("Textures", &m_ui_context.rendering.textures);
#endif
  ImGui::Checkbox("Culling", &m_ui_context.rendering.culling);
  ImGui::Checkbox("Wireframe", &m_ui_context.rendering.wireframe);
  ImGui::Checkbox("Passable", &m_ui_context.rendering.passable);
  ImGui::Checkbox("Terrain", &m_ui_context.rendering.terrain);
  ImGui::Checkbox("Static Meshes", &m_ui_context.rendering.static_meshes);
  ImGui::Checkbox("CSG", &m_ui_context.rendering.csg);
  ImGui::Checkbox("Bounding Boxes", &m_ui_context.rendering.bounding_boxes);
  ImGui::Checkbox("Imported Geodata", &m_ui_context.rendering.imported_geodata);
  ImGui::Checkbox("Generated Geodata",
                  &m_ui_context.rendering.generated_geodata);
  ImGui::End();
}

void UISystem::geodata_window() const {
  ImGui::Begin("Geodata", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

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

  ImGui::SameLine();

  ImGui::Checkbox("Export", &m_ui_context.geodata.should_export);

  ImGui::End();
}

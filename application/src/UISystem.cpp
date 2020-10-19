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

  m_ui_context.input.passable = true;
  m_ui_context.input.terrain = true;
  m_ui_context.input.static_meshes = true;
  m_ui_context.input.csg = true;
  m_ui_context.input.geodata = true;
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

  if (m_window_context.keyboard.m) {
    m_ui_context.input.wireframe = !m_ui_context.input.wireframe;
  }

  const auto &camera_position = m_rendering_context.camera.position();

  ImGui::Begin("Rendering", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("CPU frame time: %f", frame_time.seconds());
  ImGui::Text("Draws: %d", m_ui_context.output.draws);
  ImGui::Text("Camera");
  ImGui::Text("\tx: %d", static_cast<int>(camera_position.x));
  ImGui::Text("\ty: %d", static_cast<int>(camera_position.y));
  ImGui::Text("\tz: %d", static_cast<int>(camera_position.z));
  ImGui::Checkbox("Wireframe", &m_ui_context.input.wireframe);
  ImGui::Checkbox("Passable", &m_ui_context.input.passable);
  ImGui::Checkbox("Terrain", &m_ui_context.input.terrain);
  ImGui::Checkbox("Static Meshes", &m_ui_context.input.static_meshes);
  ImGui::Checkbox("CSG", &m_ui_context.input.csg);
  ImGui::Checkbox("Bounding Boxes", &m_ui_context.input.bounding_boxes);
  ImGui::Checkbox("Geodata", &m_ui_context.input.geodata);
  ImGui::End();
}

void UISystem::frame_end(Timestep /*frame_time*/) {
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#include "pch.h"

#include "Entity.h"
#include "RenderingSystem.h"

RenderingSystem::RenderingSystem(RenderingContext &rendering_context,
                                 WindowContext &window_context,
                                 UIContext &ui_context)
    : m_rendering_context{rendering_context}, m_window_context{window_context},
      m_ui_context{ui_context}, m_renderer{create_renderer()} {}

void RenderingSystem::frame_begin(Timestep /*frame_time*/) {
  m_renderer.resize(m_window_context.framebuffer.size.width,
                    m_window_context.framebuffer.size.height);

  m_renderer.clear();
}

void RenderingSystem::frame_end(Timestep /*frame_time*/) {
  rendering::FrameSettings settings{};
  settings.wireframe = m_ui_context.input.wireframe;

  if (m_ui_context.input.passable) {
    settings.surface_filter |= SURFACE_PASSABLE;
  }

  if (m_ui_context.input.terrain) {
    settings.surface_filter |= SURFACE_TERRAIN;
  }

  if (m_ui_context.input.static_meshes) {
    settings.surface_filter |= SURFACE_STATIC_MESH;
  }

  if (m_ui_context.input.csg) {
    settings.surface_filter |= SURFACE_CSG;
  }

  if (m_ui_context.input.bounding_boxes) {
    settings.surface_filter |= SURFACE_BOUNDING_BOX;
  }

  m_renderer.render(m_rendering_context.tree, settings,
                    m_ui_context.output.draws);
}

auto RenderingSystem::create_renderer() const -> rendering::Renderer {
  ASSERT(glewInit() == GLEW_OK, "App", "Can't initialize GLEW");

  GL_CALL(const auto gl_version = glGetString(GL_VERSION));
  GL_CALL(const auto gl_vendor = glGetString(GL_VENDOR));

  utils::Log(utils::LOG_INFO, "App")
      << "GL Version: " << gl_version << std::endl;
  utils::Log(utils::LOG_INFO, "App") << "GL Vendor: " << gl_vendor << std::endl;

  return rendering::Renderer{m_rendering_context.context,
                             m_rendering_context.camera};
}

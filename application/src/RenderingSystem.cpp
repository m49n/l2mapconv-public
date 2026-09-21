#include "pch.h"

#include "Entity.h"
#include "RenderingSystem.h"
#include "SurfaceVisibility.h"

RenderingSystem::RenderingSystem(RenderingContext &rendering_context,
                                 WindowContext &window_context,
                                 UIContext &ui_context)
    : m_rendering_context{rendering_context}, m_window_context{window_context},
      m_ui_context{ui_context}, m_entity_renderer{rendering::EntityRenderer{
                                    m_rendering_context.context,
                                    m_rendering_context.camera}} {

  ASSERT(glewInit() == GLEW_OK, "App", "Can't initialize GLEW");

  GL_CALL(const auto gl_version = glGetString(GL_VERSION));
  GL_CALL(const auto gl_vendor = glGetString(GL_VENDOR));

  utils::Log(utils::LOG_INFO, "App")
      << "GL Version: " << gl_version << std::endl;
  utils::Log(utils::LOG_INFO, "App") << "GL Vendor: " << gl_vendor << std::endl;

  GL_CALL(glEnable(GL_DEPTH_TEST));
  GL_CALL(glEnable(GL_BLEND));
  GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
}

void RenderingSystem::frame_begin(Timestep /*frame_time*/) {
  resize();
  clear();
}

void RenderingSystem::frame_end(Timestep /*frame_time*/) {
  rendering::FrameSettings settings{};
  settings.wireframe = m_ui_context.rendering.wireframe;
  settings.culling = m_ui_context.rendering.culling;
  settings.surface_filter = surface_filter({
      .passable = m_ui_context.rendering.passable,
      .terrain = m_ui_context.rendering.terrain,
      .static_meshes = m_ui_context.rendering.static_meshes,
      .csg = m_ui_context.rendering.csg,
      .blocking_volumes = m_ui_context.rendering.blocking_volumes,
      .bounding_boxes = m_ui_context.rendering.bounding_boxes,
      .imported_geodata = m_ui_context.rendering.imported_geodata,
      .generated_geodata = m_ui_context.rendering.generated_geodata,
  });

  if (settings.wireframe) {
    GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
  } else {
    GL_CALL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
  }

  if (m_ui_context.rendering.textures) {
    settings.surface_textures |= SURFACE_STATIC_MESH;
    settings.surface_textures |= SURFACE_TERRAIN;
    settings.surface_textures |= SURFACE_CSG;
  }

  settings.surface_textures |= SURFACE_IMPORTED_GEODATA;
  settings.surface_textures |= SURFACE_GENERATED_GEODATA;

  m_ui_context.rendering.draws = 0;

  m_entity_renderer.render(m_rendering_context.scene, settings,
                           m_ui_context.rendering.draws);
}

void RenderingSystem::resize() const {
  const auto height = m_window_context.framebuffer.size.height;
  const auto width = m_window_context.framebuffer.size.width;

  auto &framebuffer = m_rendering_context.context.framebuffer;

  if (framebuffer.size.width != width || framebuffer.size.height != height) {
    GL_CALL(glViewport(0, 0, width, height));
    framebuffer.size = {width, height};
  }
}

void RenderingSystem::clear() const {
  GL_CALL(glClearColor(0.1f, 0.1f, 0.1f, 1.0f));
  GL_CALL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
}

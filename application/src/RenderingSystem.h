#pragma once

#include "RenderingContext.h"
#include "System.h"
#include "Timestep.h"
#include "UIContext.h"
#include "LiveVisualRenderer.h"
#include "WindowContext.h"

#include <rendering/EntityRenderer.h>

class RenderingSystem : public System {
public:
  explicit RenderingSystem(RenderingContext &rendering_context,
                           WindowContext &window_context,
                           UIContext &ui_context,
                           LiveVisualRenderer &live_renderer);

  virtual void frame_begin(Timestep frame_time) override;
  virtual void frame_end(Timestep frame_time) override;

private:
  RenderingContext &m_rendering_context;
  WindowContext &m_window_context;
  UIContext &m_ui_context;
  LiveVisualRenderer &m_live_renderer;

  const rendering::EntityRenderer m_entity_renderer;

  void resize() const;
  void clear() const;
};

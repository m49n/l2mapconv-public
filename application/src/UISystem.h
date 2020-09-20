#pragma once

#include "RenderingContext.h"
#include "System.h"
#include "Timestep.h"
#include "UIContext.h"
#include "WindowContext.h"

class UISystem : public System {
public:
  explicit UISystem(UIContext &ui_context, WindowContext &window_context,
                    RenderingContext &rendering_context);
  virtual ~UISystem();

  virtual void frame_begin(Timestep frame_time) override;
  virtual void frame_end(Timestep frame_time) override;

private:
  UIContext &m_ui_context;
  WindowContext &m_window_context;
  RenderingContext &m_rendering_context;
};

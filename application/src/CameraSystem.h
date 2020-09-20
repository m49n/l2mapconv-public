#pragma once

#include "RenderingContext.h"
#include "System.h"
#include "Timestep.h"
#include "UIContext.h"
#include "WindowContext.h"

class CameraSystem : public System {
public:
  explicit CameraSystem(RenderingContext &rendering_context,
                        WindowContext &window_context, UIContext &ui_context);

  virtual void frame_begin(Timestep frame_time) override;

private:
  RenderingContext &m_rendering_context;
  WindowContext &m_window_context;
  UIContext &m_ui_context;
};

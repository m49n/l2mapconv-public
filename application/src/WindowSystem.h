#pragma once

#include "ApplicationContext.h"
#include "System.h"
#include "Timestep.h"
#include "UIContext.h"
#include "WindowContext.h"

#include <GLFW/glfw3.h>

#include <string>

class WindowSystem : public System {
public:
  explicit WindowSystem(WindowContext &window_context,
                        ApplicationContext &application_context,
                        UIContext &ui_context, const std::string &title,
                        int width, int height);
  virtual ~WindowSystem();

  virtual void start() override;

  virtual void frame_begin(Timestep frame_time) override;
  virtual void frame_end(Timestep frame_time) override;

private:
  WindowContext &m_window_context;
  ApplicationContext &m_application_context;
  UIContext &m_ui_context;
  GLFWwindow *m_window;

  auto create_window(const std::string &title, int width, int height) const
      -> GLFWwindow *;
};

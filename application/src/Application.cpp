#include "pch.h"

#include "Application.h"
#include "ApplicationContext.h"
#include "CameraSystem.h"
#include "LoadingSystem.h"
#include "RenderingContext.h"
#include "RenderingSystem.h"
#include "UIContext.h"
#include "UISystem.h"
#include "WindowContext.h"
#include "WindowSystem.h"

Application::Application(const std::vector<std::string> &arguments)
    : m_arguments{arguments} {}

auto Application::run() -> int {
  if (m_arguments.size() < 2) {
    std::cout << "Usage:" << std::endl;
    std::cout << "\tl2mapconv <L2 root path> [<map package name>]" << std::endl;
    return EXIT_FAILURE;
  }

  const auto root_path = m_arguments[0];

  auto maps = m_arguments;
  maps.erase(maps.begin());

  // Make sure to remove systems & contexts before OpenGL context will be
  // destroyed.
  {
    std::vector<std::unique_ptr<System>> systems;

    ApplicationContext application_context{};
    WindowContext window_context{};
    UIContext ui_context{};
    RenderingContext rendering_context{};

    application_context.running = true;

    systems.push_back(
        std::make_unique<WindowSystem>(window_context, application_context,
                                       ui_context, "l2mapconv", 960, 720));
    systems.push_back(std::make_unique<UISystem>(ui_context, window_context,
                                                 rendering_context));
    systems.push_back(std::make_unique<RenderingSystem>(
        rendering_context, window_context, ui_context));
    systems.push_back(std::make_unique<CameraSystem>(
        rendering_context, window_context, ui_context));
    systems.push_back(
        std::make_unique<LoadingSystem>(rendering_context, root_path, maps));

    for (const auto &system : systems) {
      system->start();
    }

    auto last_time = 0.0f;

    while (application_context.running) {
      // Shouldn't use glfwGetTime here, but it's the easiest way to get time.
      const auto time = static_cast<float>(glfwGetTime());
      Timestep frame_time{time - last_time};
      last_time = time;

      for (const auto &system : systems) {
        system->frame_begin(frame_time);
      }

      for (auto system = systems.rbegin(); system != systems.rend(); ++system) {
        (*system)->frame_end(frame_time);
      }
    }

    for (auto system = systems.rbegin(); system != systems.rend(); ++system) {
      (*system)->stop();
    }
  }

  return EXIT_SUCCESS;
}

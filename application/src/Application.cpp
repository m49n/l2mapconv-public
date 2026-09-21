#include "pch.h"

#include "Application.h"
#include "ApplicationContext.h"
#include "CameraSystem.h"
#include "GeodataContext.h"
#include "GeodataSystem.h"
#include "LoadingSystem.h"
#include "RenderingContext.h"
#include "RenderingSystem.h"
#include "UIContext.h"
#include "UISystem.h"
#include "WindowContext.h"
#include "WindowSystem.h"

Application::Application() {}

void Application::preview(const std::filesystem::path &client_root,
                          const std::vector<std::string> &maps) const {

  // Make sure to remove systems & contexts before OpenGL context will be
  // destroyed
  {
    // Initialize contexts
    ApplicationContext application_context{};
    WindowContext window_context{};
    UIContext ui_context{};
    RenderingContext rendering_context{};
    GeodataContext geodata_context{};

    Renderer renderer{rendering_context};

    // Initialize systems
    std::vector<std::unique_ptr<System>> systems;

    systems.push_back(std::make_unique<WindowSystem>(
        window_context, application_context, "l2mapconv", 1440, 1000));
    systems.push_back(std::make_unique<UISystem>(ui_context, window_context,
                                                 rendering_context));
    systems.push_back(std::make_unique<RenderingSystem>(
        rendering_context, window_context, ui_context));
    systems.push_back(std::make_unique<CameraSystem>(
        rendering_context, window_context, ui_context));
    systems.push_back(std::make_unique<LoadingSystem>(
        geodata_context, &renderer, client_root, maps));
    systems.push_back(std::make_unique<GeodataSystem>(geodata_context,
                                                      ui_context, &renderer));

    // Run application
    application_context.running = true;

    for (const auto &system : systems) {
      system->start();
    }

    auto last_frmae_time = 0.0f;

    while (application_context.running) {
      // Shouldn't use glfwGetTime here, but it's the easiest way to get time
      const auto time = static_cast<float>(glfwGetTime());
      Timestep frame_time{time - last_frmae_time};
      last_frmae_time = time;

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
}

void Application::build(const std::filesystem::path &client_root,
                        const std::vector<std::string> &maps) const {

  for (const auto &map : maps) {
    UIContext ui_context{};
    GeodataContext geodata_context{};

    LoadingSystem loading_system{geodata_context, nullptr, client_root, {map}};
    GeodataSystem geodata_system{geodata_context, ui_context, nullptr};

    ui_context.geodata.set_defaults();

    ui_context.geodata.should_export = true;
    ui_context.geodata.build_handler();
  }

  std::cout << "Done!" << std::endl;
}

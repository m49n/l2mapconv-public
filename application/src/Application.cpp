#include "pch.h"

#include "Application.h"
#include "ApplicationContext.h"
#include "CameraSystem.h"
#include "GeodataContext.h"
#include "GeodataSystem.h"
#include "LoadingSystem.h"
#include "MapCatalog.h"
#include "MapLoadingWorker.h"
#include "MapSelectionContext.h"
#include "MapStreamingSystem.h"
#include "RendererMapSceneSink.h"
#include "RenderingContext.h"
#include "RenderingSystem.h"
#include "SystemStack.h"
#include "UIContext.h"
#include "UISystem.h"
#include "WindowContext.h"
#include "WindowSystem.h"

#include "UnrealMapSource.h"

#include <algorithm>

Application::Application(std::filesystem::path resource_root)
    : m_resource_root{std::move(resource_root)} {}

void Application::preview(const std::filesystem::path &client_root,
                          const std::vector<std::string> &maps) const {

  if (maps.empty()) {
    utils::Log(utils::LOG_ERROR, "App")
        << "Preview requires at least one seed map" << std::endl;
    return;
  }

  auto catalog = MapCatalog::discover(client_root);
  std::vector<MapCoordinate> startup_coordinates;
  for (const auto &map_name : maps) {
    const auto region =
        std::find_if(catalog.regions().begin(), catalog.regions().end(),
                     [&map_name](const MapRegion &candidate) {
                       return candidate.name == map_name;
                     });
    if (region == catalog.regions().end()) {
      utils::Log(utils::LOG_ERROR, "App")
          << "Map is not present in the client catalog: " << map_name
          << std::endl;
      return;
    }
    startup_coordinates.push_back(region->coordinate);
  }

  utils::Log(utils::LOG_INFO, "App")
      << "Map catalog count=" << catalog.regions().size() << std::endl;
  MapSelectionContext map_selection{std::move(catalog)};
  for (const auto coordinate : startup_coordinates) {
    map_selection.set_manual(coordinate, true);
  }
  const auto seed_coordinate = startup_coordinates.front();

  ApplicationContext application_context{};
  WindowContext window_context{};
  WindowSystem window_system{window_context, application_context, "l2mapconv",
                             1440, 1000};
  window_system.start();

  // GPU resources and background workers live in this inner scope so they are
  // destroyed before WindowSystem terminates GLFW and its OpenGL context.
  {
    UIContext ui_context{};
    ui_context.geodata.streaming_preview = true;
    RenderingContext rendering_context{};
    GeodataContext geodata_context{};

    Renderer renderer{rendering_context, m_resource_root};

    SystemStack systems;
    systems.push(std::make_unique<CameraSystem>(rendering_context,
                                                window_context, ui_context));
    systems.push(std::make_unique<MapStreamingSystem>(
        map_selection,
        std::make_unique<MapLoadingWorker>(
            std::make_unique<UnrealMapSource>(client_root)),
        std::make_unique<RendererMapSceneSink>(renderer, rendering_context),
        seed_coordinate));
    systems.push(std::make_unique<UISystem>(ui_context, window_context,
                                            rendering_context, map_selection));
    systems.push(std::make_unique<RenderingSystem>(rendering_context,
                                                   window_context, ui_context));
    systems.push(std::make_unique<GeodataSystem>(geodata_context, ui_context,
                                                 &renderer));

    // Run application
    application_context.running = true;

    systems.start();

    auto last_frmae_time = 0.0f;

    while (application_context.running) {
      // Shouldn't use glfwGetTime here, but it's the easiest way to get time
      const auto time = static_cast<float>(glfwGetTime());
      Timestep frame_time{time - last_frmae_time};
      last_frmae_time = time;

      window_system.frame_begin(frame_time);
      systems.frame_begin(frame_time);
      systems.frame_end(frame_time);
      window_system.frame_end(frame_time);
    }
    systems.shutdown();
  }
  window_system.stop();
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

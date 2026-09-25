#include "pch.h"

#include "Application.h"
#include "BuildIdentity.h"
#include "ClientFolderPicker.h"
#include "ClientStartup.h"
#include "CommandLine.h"
#include "DesktopStartup.h"
#include "ExecutablePath.h"
#include "RecentClients.h"
#include "TerritoryCommandLine.h"

#include <cstdlib>
#include <exception>

namespace {

auto local_app_data_root() -> std::optional<std::filesystem::path> {
#ifdef _WIN32
  const auto *value = _wgetenv(L"LOCALAPPDATA");
  if (value == nullptr || *value == L'\0') {
    return std::nullopt;
  }
  return std::filesystem::path{value};
#else
  const auto *value = std::getenv("LOCALAPPDATA");
  if (value == nullptr || *value == '\0') {
    return std::nullopt;
  }
  return std::filesystem::path{value};
#endif
}

void save_recent_clients(
    const RecentClients &recent_clients,
    const std::optional<std::filesystem::path> &settings_file) {
  if (settings_file && !recent_clients.save(*settings_file)) {
    utils::Log(utils::LOG_WARN, "App")
        << "Unable to save recent clients to " << *settings_file << std::endl;
  }
}

auto run_preview_sessions(
    const Application &application, RecentClients &recent_clients,
    const std::optional<std::filesystem::path> &settings_file,
    ClientStartupSelection selection, std::vector<std::string> maps) -> int {
  while (true) {
    recent_clients.promote(selection.client_root);
    save_recent_clients(recent_clients, settings_file);

    auto result = application.preview(selection.client_root, maps,
                                      recent_clients.entries(),
                                      choose_client_root_folder);
    if (!result.requested_client) {
      return EXIT_SUCCESS;
    }

    selection = std::move(*result.requested_client);
    maps = {selection.seed_map};
  }
}

auto run_desktop(const Application &application) -> int {
  const auto settings_file =
      recent_clients_settings_path(local_app_data_root());
  auto recent_clients =
      settings_file ? RecentClients::load(*settings_file) : RecentClients{};
  if (!settings_file) {
    utils::Log(utils::LOG_WARN, "App")
        << "LOCALAPPDATA is unavailable; recent client persistence is "
           "disabled"
        << std::endl;
  }

  auto selection = choose_desktop_client(
      recent_clients.entries(), choose_client_root_folder,
      [](std::string_view message) { show_client_selection_error(message); });
  if (!selection) {
    return EXIT_SUCCESS;
  }

  std::vector<std::string> maps{selection->seed_map};
  return run_preview_sessions(application, recent_clients, settings_file,
                              std::move(*selection), std::move(maps));
}

} // namespace

auto main(int argc, char **argv) -> int {
  if (auto result = dispatch_territory_command(territory_utf8_arguments(argc, argv))) {
    return *result;
  }
  const Application application{running_executable_directory()};
  const std::vector<std::string> arguments{argv, argv + argc};
  if (is_desktop_invocation(arguments)) {
    utils::Log::level = utils::LOG_INFO;
    utils::Log::colored = false;
    return run_desktop(application);
  }

  // Define options
  cxxopts::Options options{argv[0]};

  options                                                                    //
      .custom_help("--preview/build --client-root <path> -- [maps...]")      //
      .allow_unrecognised_options()                                          //
      .add_options()                                                         //
                                                                             //
      ("preview", "Preview maps")                                            //
                                                                             //
      ("build", "Build maps (writes L2J and PTS files to `output`)")         //
                                                                             //
      ("client-root", "Path to the Lineage II client",                       //
       cxxopts::value<std::string>())                                        //
                                                                             //
      ("log-level",                                                          //
       "Log level (0 - none, 1 - fatal, 2 - error, 3 - warn, 4 - info, 5 - " //
       "debug, 6 - all)",                                                    //
       cxxopts::value<unsigned int>()->default_value("3"))                   //
                                                                             //
      ("help", "Print help")                                               //
      ("version", "Print build identity");

  // Parse options
  const auto &input = options.parse(argc, argv);

  if (input.count("version") > 0) {
    std::cout << build_identity << std::endl;
    return EXIT_SUCCESS;
  }

  // Help
  if (input.count("help") > 0) {
    std::cout << options.help() << std::endl;
    std::cout << "Territory tools (stdout: one JSON result; exit 0/2/3/130):\n"
                 "  --render-territory --client-root <sam> --output <dir> [--resolution 1024|2048|4096|8192|16384] [--no-water] [--no-textures] [--shadows [--sun-azimuth 0..360] [--sun-elevation 15..80]] -- dd_dd [...]\n"
                 "  --inspect-territory --client-root <sam> --output <dir> -- dd_dd [...]\n";
    return EXIT_SUCCESS;
  }

  // Logging
  const auto log_level =
      static_cast<utils::LogLevel>(input["log-level"].as<unsigned int>());
  utils::Log::level = log_level;
  utils::Log::colored = false;

  // Commands
  auto preview = false;
  auto build = false;
  if (input.count("preview") > 0) {
    preview = true;
  } else if (input.count("build") > 0) {
    build = true;
  } else {
    utils::Log(utils::LOG_ERROR)
        << "Unspecified command (use either --preview or --build)" << std::endl;
    std::cout << options.help() << std::endl;
    return EXIT_FAILURE;
  }

  // Client root
  if (input.count("client-root") == 0) {
    utils::Log(utils::LOG_ERROR)
        << "Unspecified Lineage II client path (--client-root)" << std::endl;
    std::cout << options.help() << std::endl;
    return EXIT_FAILURE;
  }

  const auto client_root =
      client_root_path(input["client-root"].as<std::string>());
  if (!std::filesystem::exists(client_root)) {
    utils::Log(utils::LOG_ERROR)
        << "Invalid Lineage II client path: " << client_root << std::endl;
    return EXIT_FAILURE;
  }

  // Maps
  const auto &maps = input.unmatched();
  if (maps.empty()) {
    utils::Log(utils::LOG_ERROR) << "No maps provided" << std::endl;
    std::cout << options.help() << std::endl;
    return EXIT_FAILURE;
  }

  // Run application
  if (preview) {
    auto selection = inspect_client_root(client_root);
    if (!selection) {
      utils::Log(utils::LOG_ERROR)
          << "Invalid Lineage II preview client path: " << client_root
          << std::endl;
      return EXIT_FAILURE;
    }

    const auto settings_file =
        recent_clients_settings_path(local_app_data_root());
    auto recent_clients =
        settings_file ? RecentClients::load(*settings_file) : RecentClients{};
    if (!settings_file) {
      utils::Log(utils::LOG_WARN, "App")
          << "LOCALAPPDATA is unavailable; recent client persistence is "
             "disabled"
          << std::endl;
    }
    return run_preview_sessions(application, recent_clients, settings_file,
                                std::move(*selection), maps);
  }
  if (build) {
    try {
      application.build(client_root, maps);
      return EXIT_SUCCESS;
    } catch (const std::exception &error) {
      std::cerr << "Geodata build failed: " << error.what() << std::endl;
      return EXIT_FAILURE;
    }
  }

  ASSERT(false, "App", "Unknown command");
  return EXIT_FAILURE;
}

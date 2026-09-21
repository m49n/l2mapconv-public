#include "pch.h"

#include "Application.h"
#include "CommandLine.h"

auto main(int argc, char **argv) -> int {
  // Define options
  cxxopts::Options options{argv[0]};

  options                                                                    //
      .custom_help("--preview/build --client-root <path> -- [maps...]")      //
      .allow_unrecognised_options()                                          //
      .add_options()                                                         //
                                                                             //
      ("preview", "Preview maps")                                            //
                                                                             //
      ("build", "Build maps (see results in the `output` directory)")        //
                                                                             //
      ("client-root", "Path to the Lineage II client",                       //
       cxxopts::value<std::string>())                                        //
                                                                             //
      ("log-level",                                                          //
       "Log level (0 - none, 1 - fatal, 2 - error, 3 - warn, 4 - info, 5 - " //
       "debug, 6 - all)",                                                    //
       cxxopts::value<unsigned int>()->default_value("3"))                   //
                                                                             //
      ("help", "Print help");

  // Parse options
  const auto &input = options.parse(argc, argv);

  // Help
  if (input.count("help") > 0) {
    std::cout << options.help() << std::endl;
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
  const Application application{executable_directory(argv[0])};
  if (preview) {
    application.preview(client_root, maps);
  } else if (build) {
    application.build(client_root, maps);
  } else {
    ASSERT(false, "App", "Unknown command");
  }
}

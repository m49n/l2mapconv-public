#include "pch.h"

#include "Application.h"

auto main(int argc, char **argv) -> int {
  //  utils::Log::level = utils::LOG_ALL;

  Application application{{argv + 1, argv + argc}};
  return application.run();
}

#pragma once

#include <filesystem>

auto running_executable_path() -> std::filesystem::path;

inline auto running_executable_directory() -> std::filesystem::path {
  return running_executable_path().parent_path();
}

#pragma once

#include <filesystem>
#include <string_view>

inline auto client_root_path(std::string_view value) -> std::filesystem::path {
  return std::filesystem::path{value};
}

#pragma once

#include <imgui.h>

#include <cstdlib>
#include <filesystem>
#include <string>

inline auto imgui_ini_filename() -> const char * {
  // ImGui retains the pointer and saves periodically and on context shutdown.
  static const std::string filename = []() -> std::string {
#ifdef _WIN32
    const auto *root = _wgetenv(L"LOCALAPPDATA");
#else
    const auto *root = std::getenv("LOCALAPPDATA");
#endif
    if (root == nullptr || *root == 0)
      return {};

    try {
      const auto directory = std::filesystem::path{root} / "l2mapconv";
      if (!directory.is_absolute())
        return {};
      std::error_code error;
      std::filesystem::create_directories(directory, error);
      if (error)
        return {};
      const auto encoded = (directory / "imgui.ini").u8string();
      return {encoded.begin(), encoded.end()};
    } catch (const std::filesystem::filesystem_error &) {
      return {};
    }
  }();
  return filename.empty() ? nullptr : filename.c_str();
}

enum class UIWindow { Geodata, Client, Maps, TerritoryRender, Pathfinding, Rendering };

inline void prepare_ui_window(UIWindow window) {
  // FirstUseEver yields to the saved position and collapsed state per window.
  ImGui::SetNextWindowPos(
      {10.f, 10.f + static_cast<float>(window) * ImGui::GetFrameHeight()},
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowCollapsed(window != UIWindow::Rendering,
                                ImGuiCond_FirstUseEver);
}

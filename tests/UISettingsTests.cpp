#include "TestSupport.h"
#include "UISettings.h"

#include <imgui.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>

namespace {

class SettingsDirectory {
public:
  SettingsDirectory()
      : path{std::filesystem::temp_directory_path() /
             ("l2mapconv-ui-" + std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count())) /
             std::filesystem::path{u8"профиль"}} {
#ifdef _WIN32
    if (const auto *value = _wgetenv(L"LOCALAPPDATA"))
      previous = value;
    _wputenv_s(L"LOCALAPPDATA", path.c_str());
#else
    if (const auto *value = std::getenv("LOCALAPPDATA"))
      previous = value;
    setenv("LOCALAPPDATA", path.c_str(), 1);
#endif
  }

  ~SettingsDirectory() {
#ifdef _WIN32
    _wputenv_s(L"LOCALAPPDATA", previous ? previous->c_str() : L"");
#else
    if (previous)
      setenv("LOCALAPPDATA", previous->c_str(), 1);
    else
      unsetenv("LOCALAPPDATA");
#endif
    std::error_code ignored;
    std::filesystem::remove_all(path.parent_path(), ignored);
  }

  std::filesystem::path path;
  std::optional<std::filesystem::path::string_type> previous;
};

void create_ui(const char *filename) {
  ImGui::CreateContext();
  auto &io = ImGui::GetIO();
  io.IniFilename = filename;
  io.DisplaySize = {1920.f, 1080.f};
  io.DeltaTime = 1.f / 60.f;
  unsigned char *pixels = nullptr;
  int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
}

} // namespace

int main() {
  const SettingsDirectory settings;
  const auto *filename = imgui_ini_filename();
  auto failures = expect(filename != nullptr,
                         "window layout persistence has a settings file");
  if (filename == nullptr)
    return failures;

  const auto expected_file = settings.path / "l2mapconv" / "imgui.ini";
  const auto encoded_file = expected_file.u8string();
  failures += expect(std::string{filename} ==
                         std::string{encoded_file.begin(), encoded_file.end()},
                     "layout uses a Unicode-safe user settings path");

  create_ui(filename);
  ImGui::NewFrame();
  struct Window {
    UIWindow window;
    const char *name;
    bool collapsed;
  };
  constexpr std::array windows{
      Window{UIWindow::Geodata, "Geodata", true},
      Window{UIWindow::Client, "Client", true},
      Window{UIWindow::Maps, "Maps", true},
      Window{UIWindow::TerritoryRender, "Territory Render", true},
      Window{UIWindow::Pathfinding, "Pathfinding Lab", true},
      Window{UIWindow::Rendering, "Rendering", false}};
  float previous_bottom = 10.f;
  for (const auto &window : windows) {
    prepare_ui_window(window.window);
    ImGui::SetNextWindowSize({400.f, 300.f}, ImGuiCond_FirstUseEver);
    ImGui::Begin(window.name);
    const auto position = ImGui::GetWindowPos();
    failures += expect(position.x == 10.f && position.y == previous_bottom,
                       "fresh windows form one contiguous stack at the upper left");
    failures += expect(ImGui::IsWindowCollapsed() == window.collapsed,
                       "only Rendering is initially expanded");
    previous_bottom = position.y + ImGui::GetWindowHeight();
    ImGui::End();
  }
  ImGui::EndFrame();

  ImGui::NewFrame();
  prepare_ui_window(UIWindow::Client);
  ImGui::SetNextWindowPos({260.f, 220.f});
  ImGui::SetNextWindowSize({400.f, 300.f});
  ImGui::SetNextWindowCollapsed(false);
  ImGui::Begin("Client");
  ImGui::End();
  prepare_ui_window(UIWindow::Rendering);
  ImGui::SetNextWindowPos({710.f, 310.f});
  ImGui::SetNextWindowCollapsed(true);
  ImGui::Begin("Rendering");
  ImGui::End();
  ImGui::EndFrame();
  ImGui::DestroyContext();
  failures += expect(std::filesystem::is_regular_file(expected_file),
                     "normal UI shutdown saves layout without an explicit save");

  create_ui(filename);
  ImGui::NewFrame();
  prepare_ui_window(UIWindow::Client);
  ImGui::Begin("Client");
  const auto position = ImGui::GetWindowPos();
  failures += expect(position.x == 260.f && position.y == 220.f,
                     "window position survives a UI restart");
  failures += expect(!ImGui::IsWindowCollapsed(),
                     "saved expansion overrides the initially collapsed default");
  ImGui::End();
  prepare_ui_window(UIWindow::Rendering);
  ImGui::Begin("Rendering");
  const auto rendering_position = ImGui::GetWindowPos();
  failures += expect(rendering_position.x == 710.f && rendering_position.y == 310.f,
                     "saved Rendering position overrides the default stack");
  failures += expect(ImGui::IsWindowCollapsed(),
                     "saved collapse overrides the initially expanded default");
  ImGui::End();
  ImGui::EndFrame();
  ImGui::DestroyContext();
  return failures;
}

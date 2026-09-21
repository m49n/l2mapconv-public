#include "DesktopStartup.h"
#include "TestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace {

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path = std::filesystem::temp_directory_path() /
           ("l2mapconv-desktop-startup-" + suffix);
    std::filesystem::create_directories(path / "Maps");
  }

  ~TemporaryDirectory() { std::filesystem::remove_all(path); }

  std::filesystem::path path;
};

void touch(const std::filesystem::path &path) { std::ofstream file{path}; }

} // namespace

auto run_desktop_startup_tests() -> int {
  auto failures = 0;
  failures += expect(is_desktop_invocation({"l2mapconv.exe"}),
                     "argument-free launch enters desktop mode");
  failures += expect(!is_desktop_invocation({"l2mapconv.exe", "--help"}),
                     "help remains explicit and non-interactive");
  failures += expect(!is_desktop_invocation({"l2mapconv.exe", "--preview"}),
                     "partial explicit preview does not open a picker");

  const TemporaryDirectory valid;
  touch(valid.path / "Maps" / "22_22.unr");
  auto browse_calls = 0;
  const auto initial = choose_desktop_client(
      {valid.path}, [&]() -> std::optional<std::filesystem::path> {
        ++browse_calls;
        return std::nullopt;
      });
  failures += expect(initial && initial->client_root ==
                                    std::filesystem::absolute(valid.path),
                     "valid recent client opens without browsing");
  failures +=
      expect(browse_calls == 0, "valid recent client bypasses the picker");

  const auto cancelled =
      choose_desktop_client({}, [&]() -> std::optional<std::filesystem::path> {
        ++browse_calls;
        return std::nullopt;
      });
  failures += expect(!cancelled.has_value(),
                     "cancelled first-launch picker exits cleanly");

  std::vector<std::optional<std::filesystem::path>> choices{
      valid.path / "missing", valid.path};
  auto invalid_errors = 0;
  const auto recovered = choose_desktop_client(
      {},
      [&]() mutable -> std::optional<std::filesystem::path> {
        const auto choice = choices.front();
        choices.erase(choices.begin());
        return choice;
      },
      [&](std::string_view) { ++invalid_errors; });
  failures += expect(recovered && recovered->seed_map == "22_22",
                     "invalid first-launch selection can recover");
  failures += expect(invalid_errors == 1,
                     "invalid first-launch selection reports one error");
  return failures;
}

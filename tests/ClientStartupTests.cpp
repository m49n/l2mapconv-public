#include "ClientStartup.h"
#include "TestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class TemporaryDirectory {
public:
  explicit TemporaryDirectory(std::string_view label) {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path = std::filesystem::temp_directory_path() /
           ("l2mapconv-" + std::string{label} + "-" + suffix);
    std::filesystem::create_directories(path / "Maps");
  }

  ~TemporaryDirectory() { std::filesystem::remove_all(path); }

  std::filesystem::path path;
};

void touch(const std::filesystem::path &path) { std::ofstream file{path}; }

} // namespace

auto run_client_startup_tests() -> int {
  auto failures = 0;
  const TemporaryDirectory empty{"empty client"};
  failures += expect(!inspect_client_root(empty.path).has_value(),
                     "client without numeric maps is invalid");
  failures += expect(!inspect_client_root(empty.path / "missing").has_value(),
                     "missing client directory is invalid");

  const TemporaryDirectory fallback{"fallback client with spaces"};
  touch(fallback.path / "Maps" / "19_21.unr");
  touch(fallback.path / "Maps" / "20_20.unr");
  const auto fallback_selection = inspect_client_root(fallback.path / ".");
  failures +=
      expect(fallback_selection && fallback_selection->seed_map == "19_21",
             "first sorted map is fallback seed");
  failures += expect(
      fallback_selection &&
          fallback_selection->client_root ==
              std::filesystem::absolute(fallback.path).lexically_normal(),
      "client root is returned absolute and normalized");

  const TemporaryDirectory preferred{"preferred client"};
  touch(preferred.path / "Maps" / "19_21.unr");
  touch(preferred.path / "Maps" / "22_22.unr");
  const auto preferred_selection = inspect_client_root(preferred.path);
  failures +=
      expect(preferred_selection && preferred_selection->seed_map == "22_22",
             "22_22 is preferred when available");

  const auto selected =
      first_valid_recent({empty.path / "missing", empty.path, preferred.path});
  failures += expect(
      selected &&
          selected->client_root ==
              std::filesystem::absolute(preferred.path).lexically_normal(),
      "startup skips stale recent clients");
  return failures;
}

#include "RecentClients.h"
#include "TestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

class TemporaryDirectory {
public:
  TemporaryDirectory() {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path = std::filesystem::temp_directory_path() /
           ("l2mapconv-recent-clients-" + suffix);
    std::filesystem::create_directories(path);
  }

  ~TemporaryDirectory() { std::filesystem::remove_all(path); }

  std::filesystem::path path;
};

void write_text(const std::filesystem::path &path, std::string_view text) {
  std::ofstream file{path, std::ios::binary | std::ios::trunc};
  file << text;
}

} // namespace

auto run_recent_clients_tests() -> int {
  auto failures = 0;
  const TemporaryDirectory temporary;
  const auto settings = temporary.path / "settings.ini";

  auto recent = RecentClients::load(settings);
  failures +=
      expect(recent.entries().empty(), "missing recent settings are empty");

  recent.promote(temporary.path / "Client A" / ".");
  recent.promote(temporary.path / "Client B");
  recent.promote(temporary.path / "client a");
  failures += expect(recent.entries().size() == 2,
                     "equivalent Windows paths are deduplicated");
  failures += expect(recent.entries().front().filename() == "client a",
                     "promoted client moves to the front");

  const auto unicode_client =
      temporary.path / std::filesystem::path{u8"Клиент Samurai"};
  recent.promote(unicode_client);

  for (auto index = 0; index < 12; ++index) {
    recent.promote(temporary.path / ("Client " + std::to_string(index)));
  }
  failures += expect(recent.entries().size() == 10,
                     "recent history is capped at ten clients");
  failures += expect(recent.save(settings), "recent settings save atomically");
  failures +=
      expect(RecentClients::load(settings).entries() == recent.entries(),
             "recent settings round trip with spaces and Unicode");
  failures += expect(!std::filesystem::exists(settings.string() + ".tmp"),
                     "atomic save leaves no temporary sibling");

  recent.promote(temporary.path / "Replacement Client");
  failures += expect(recent.save(settings),
                     "existing recent settings are replaced atomically");
  failures +=
      expect(RecentClients::load(settings).entries() == recent.entries(),
             "replacement settings contain the complete new history");

  write_text(settings, "version=999\nclient=C:\\unsupported\n");
  failures += expect(RecentClients::load(settings).entries().empty(),
                     "future settings versions are ignored");
  write_text(settings, "this is not settings\n");
  failures += expect(RecentClients::load(settings).entries().empty(),
                     "malformed settings are ignored");

  failures += expect(recent_clients_settings_path(temporary.path) ==
                         temporary.path / "l2mapconv" / "settings.ini",
                     "settings path is rooted in local app data");
  failures += expect(!recent_clients_settings_path(std::nullopt).has_value(),
                     "missing local app data disables persistence");
  return failures;
}

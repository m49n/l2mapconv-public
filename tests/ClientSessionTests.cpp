#include "ClientSessionContext.h"
#include "TestSupport.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

class TemporaryDirectory {
public:
  explicit TemporaryDirectory(std::string_view label) {
    const auto suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    path = std::filesystem::temp_directory_path() /
           ("l2mapconv-session-" + std::string{label} + "-" + suffix);
    std::filesystem::create_directories(path / "Maps");
  }

  ~TemporaryDirectory() { std::filesystem::remove_all(path); }

  std::filesystem::path path;
};

void touch(const std::filesystem::path &path) { std::ofstream file{path}; }

} // namespace

auto run_client_session_tests() -> int {
  auto failures = 0;
  const TemporaryDirectory valid{"valid"};
  touch(valid.path / "Maps" / "22_22.unr");
  const TemporaryDirectory invalid{"invalid"};

  auto browse_calls = 0;
  ClientSessionContext context{valid.path,
                               {valid.path, invalid.path},
                               [&]() -> std::optional<std::filesystem::path> {
                                 ++browse_calls;
                                 return std::nullopt;
                               }};
  failures += expect(!context.browse(), "cancelled picker requests no switch");
  failures += expect(browse_calls == 1, "browse invokes the picker once");
  failures += expect(!context.request_switch(invalid.path),
                     "invalid client requests no switch");
  failures += expect(!context.requested_client().has_value(),
                     "invalid switch leaves session active");
  failures += expect(!context.error().empty(),
                     "invalid switch exposes a user-visible error");
  failures += expect(context.request_switch(valid.path),
                     "valid client requests a switch");
  failures += expect(context.requested_client()->seed_map == "22_22",
                     "switch carries validated seed selection");
  failures += expect(!context.request_switch(valid.path),
                     "second switch request is ignored");
  failures += expect(!context.browse(),
                     "browse is ignored after a switch was requested");
  failures += expect(browse_calls == 1,
                     "completed switch does not invoke the picker twice");
  failures += expect(context.current_client() == valid.path,
                     "current client remains available to the UI");
  failures += expect(context.recent_clients().size() == 2,
                     "recent clients remain available to the UI");
  return failures;
}

#include "DesktopStartup.h"

auto is_desktop_invocation(const std::vector<std::string> &arguments) -> bool {
  return arguments.size() == 1;
}

auto choose_desktop_client(
    const std::vector<std::filesystem::path> &recent_clients,
    const BrowseClientRoot &browse, const ClientSelectionError &report_error)
    -> std::optional<ClientStartupSelection> {
  if (auto recent = first_valid_recent(recent_clients)) {
    return recent;
  }
  if (!browse) {
    return std::nullopt;
  }

  while (const auto selected = browse()) {
    if (auto client = inspect_client_root(*selected)) {
      return client;
    }
    if (report_error) {
      report_error("The selected directory does not contain numeric "
                   "Maps/*.unr files. Choose the client's sam directory.");
    }
  }
  return std::nullopt;
}

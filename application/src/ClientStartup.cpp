#include "ClientStartup.h"

#include "MapCatalog.h"

#include <algorithm>
#include <system_error>

auto inspect_client_root(const std::filesystem::path &client_root)
    -> std::optional<ClientStartupSelection> {
  try {
    const auto normalized =
        std::filesystem::absolute(client_root).lexically_normal();
    std::error_code error;
    if (!std::filesystem::is_directory(normalized, error) || error) {
      return std::nullopt;
    }

    const auto catalog = MapCatalog::discover(normalized);
    if (catalog.regions().empty()) {
      return std::nullopt;
    }

    const auto preferred = std::ranges::find(
        catalog.regions(), std::string{"22_22"}, &MapRegion::name);
    return ClientStartupSelection{normalized,
                                  preferred == catalog.regions().end()
                                      ? catalog.regions().front().name
                                      : preferred->name};
  } catch (const std::filesystem::filesystem_error &) {
    return std::nullopt;
  }
}

auto first_valid_recent(
    const std::vector<std::filesystem::path> &recent_clients)
    -> std::optional<ClientStartupSelection> {
  for (const auto &client_root : recent_clients) {
    if (auto selection = inspect_client_root(client_root)) {
      return selection;
    }
  }
  return std::nullopt;
}

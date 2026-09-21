#pragma once

#include "ClientStartup.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using BrowseClientRoot = std::function<std::optional<std::filesystem::path>()>;
using ClientSelectionError = std::function<void(std::string_view)>;

auto is_desktop_invocation(const std::vector<std::string> &arguments) -> bool;
auto choose_desktop_client(
    const std::vector<std::filesystem::path> &recent_clients,
    const BrowseClientRoot &browse,
    const ClientSelectionError &report_error = {})
    -> std::optional<ClientStartupSelection>;

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct ClientStartupSelection {
  std::filesystem::path client_root;
  std::string seed_map;
};

auto inspect_client_root(const std::filesystem::path &client_root)
    -> std::optional<ClientStartupSelection>;

auto first_valid_recent(
    const std::vector<std::filesystem::path> &recent_clients)
    -> std::optional<ClientStartupSelection>;

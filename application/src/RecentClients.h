#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

class RecentClients {
public:
  static constexpr std::size_t maximum_entries = 10;

  static auto load(const std::filesystem::path &settings_file) -> RecentClients;

  auto save(const std::filesystem::path &settings_file) const -> bool;
  void promote(const std::filesystem::path &client_root);
  auto entries() const -> const std::vector<std::filesystem::path> &;

private:
  std::vector<std::filesystem::path> m_entries;
};

auto recent_clients_settings_path(
    const std::optional<std::filesystem::path> &local_app_data)
    -> std::optional<std::filesystem::path>;

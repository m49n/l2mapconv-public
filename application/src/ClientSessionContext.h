#pragma once

#include "ClientStartup.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class ClientSessionContext {
public:
  using BrowseHandler = std::function<std::optional<std::filesystem::path>()>;

  ClientSessionContext(std::filesystem::path current_client,
                       std::vector<std::filesystem::path> recent_clients,
                       BrowseHandler browse_handler);

  auto browse() -> bool;
  auto request_switch(const std::filesystem::path &client_root) -> bool;
  auto current_client() const -> const std::filesystem::path &;
  auto recent_clients() const -> const std::vector<std::filesystem::path> &;
  auto requested_client() const
      -> const std::optional<ClientStartupSelection> &;
  auto error() const -> const std::string &;

private:
  std::filesystem::path m_current_client;
  std::vector<std::filesystem::path> m_recent_clients;
  BrowseHandler m_browse_handler;
  std::optional<ClientStartupSelection> m_requested_client;
  std::string m_error;
};

struct PreviewSessionResult {
  std::optional<ClientStartupSelection> requested_client;
};

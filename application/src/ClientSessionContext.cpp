#include "ClientSessionContext.h"

#include <utility>

ClientSessionContext::ClientSessionContext(
    std::filesystem::path current_client,
    std::vector<std::filesystem::path> recent_clients,
    BrowseHandler browse_handler)
    : m_current_client{std::move(current_client)},
      m_recent_clients{std::move(recent_clients)},
      m_browse_handler{std::move(browse_handler)} {}

auto ClientSessionContext::browse() -> bool {
  if (m_requested_client) {
    return false;
  }
  if (!m_browse_handler) {
    m_error = "Folder selection is unavailable for this preview session.";
    return false;
  }

  const auto selected = m_browse_handler();
  return selected && request_switch(*selected);
}

auto ClientSessionContext::request_switch(
    const std::filesystem::path &client_root) -> bool {
  if (m_requested_client) {
    return false;
  }

  auto selection = inspect_client_root(client_root);
  if (!selection) {
    m_error = "The selected directory does not contain numeric Maps/*.unr "
              "files. Choose the client's sam directory.";
    return false;
  }

  m_error.clear();
  m_requested_client = std::move(selection);
  return true;
}

auto ClientSessionContext::current_client() const
    -> const std::filesystem::path & {
  return m_current_client;
}

auto ClientSessionContext::recent_clients() const
    -> const std::vector<std::filesystem::path> & {
  return m_recent_clients;
}

auto ClientSessionContext::requested_client() const
    -> const std::optional<ClientStartupSelection> & {
  return m_requested_client;
}

auto ClientSessionContext::error() const -> const std::string & {
  return m_error;
}

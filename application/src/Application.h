#pragma once

#include "ClientSessionContext.h"

#include <filesystem>
#include <string>
#include <vector>

class Application {
public:
  explicit Application(std::filesystem::path resource_root);

  auto preview(const std::filesystem::path &client_root,
               const std::vector<std::string> &maps,
               std::vector<std::filesystem::path> recent_clients = {},
               ClientSessionContext::BrowseHandler browse_handler = {}) const
      -> PreviewSessionResult;
  void build(const std::filesystem::path &client_root,
             const std::vector<std::string> &maps) const;

private:
  const std::filesystem::path m_resource_root;
};

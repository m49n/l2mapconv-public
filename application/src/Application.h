#pragma once

#include <filesystem>
#include <string>
#include <vector>

class Application {
public:
  explicit Application(std::filesystem::path resource_root);

  void preview(const std::filesystem::path &client_root,
               const std::vector<std::string> &maps) const;
  void build(const std::filesystem::path &client_root,
             const std::vector<std::string> &maps) const;

private:
  const std::filesystem::path m_resource_root;
};

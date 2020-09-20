#pragma once

#include "ArchiveLoader.h"
#include "Package.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace unreal {

class PackageLoader {
public:
  explicit PackageLoader(const std::filesystem::path &root_path,
                         const std::vector<SearchConfig> &configs)
      : m_archive_loader{root_path, configs} {}

  auto load_package(const std::string &name) const -> std::optional<Package>;

private:
  ArchiveLoader m_archive_loader;
};

} // namespace unreal

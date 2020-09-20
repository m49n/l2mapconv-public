#include "pch.h"

#include <unreal/PackageLoader.h>

namespace unreal {

auto PackageLoader::load_package(const std::string &name) const
    -> std::optional<Package> {

  auto *archive = m_archive_loader.load_archive(name);

  if (archive == nullptr) {
    return {};
  }

  return Package{*archive};
}

} // namespace unreal

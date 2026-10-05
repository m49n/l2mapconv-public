#pragma once

#include "Archive.h"
#include "NameTable.h"

#include <filesystem>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace unreal {
enum class ArchiveReadPhase { Before, After };
using ArchiveReadObserver = std::function<void(const std::filesystem::path&, ArchiveReadPhase)>;

struct SearchConfig {
  const std::filesystem::path directory;
  const std::string extension;

  explicit SearchConfig(const std::string &directory,
                        const std::string &extension)
      : directory{directory}, extension{extension} {}
};

class ArchiveLoader {
public:
  explicit ArchiveLoader(const std::filesystem::path &root_path,
                         const std::vector<SearchConfig> &configs,
                         ArchiveReadObserver observer = {})
      : m_root_path{root_path}, m_configs{configs}, m_observer{std::move(observer)} {}

  auto load_archive(const std::string &name) const -> Archive *;

private:
  const std::filesystem::path m_root_path;
  const std::vector<SearchConfig> m_configs;
  ArchiveReadObserver m_observer;

  mutable std::unordered_map<std::string, Archive> m_archives;

  auto load_and_cache_archive(const std::string &name,
                              const std::filesystem::path &path) const
      -> Archive *;
};

} // namespace unreal

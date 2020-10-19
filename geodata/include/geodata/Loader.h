#pragma once

#include "Geodata.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace geodata {

class Loader {
public:
  explicit Loader(const std::filesystem::path &root_path);

  auto load_geodata(const std::string &name) const -> const Geodata *;

private:
  std::filesystem::path m_root_path;

  mutable std::unordered_map<std::string, Geodata> m_geodata;

  auto load_and_cache_l2j_geodata(const std::string &name,
                                  const std::filesystem::path &path) const
      -> Geodata *;
};

} // namespace geodata

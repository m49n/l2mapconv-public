#include "pch.h"

#include <geodata/Loader.h>

#include "L2JSerializer.h"

namespace geodata {

Loader::Loader(const std::filesystem::path &root_path)
    : m_root_path{root_path} {}

auto Loader::load_geodata(const std::string &name) const -> const Geodata * {
  const auto pair = m_geodata.find(name);

  if (pair != m_geodata.end()) {
    return &pair->second;
  }

  utils::Log(utils::LOG_INFO, "Geodata")
      << "Loading geodata: " << name << std::endl;

  const auto l2j_path = m_root_path / (name + ".l2j");

  if (std::filesystem::exists(l2j_path)) {
    return load_and_cache_l2j_geodata(name, l2j_path);
  }

  utils::Log(utils::LOG_INFO, "Geodata")
      << "Can't find geodata: " << name << std::endl;

  return nullptr;
}

auto Loader::load_and_cache_l2j_geodata(const std::string &name,
                                        const std::filesystem::path &path) const
    -> Geodata * {

  L2JSerializer serializer;
  std::ifstream input{path, std::ios::binary};
  const auto inserted =
      m_geodata.emplace(std::make_pair(name, serializer.deserialize(input)));

  utils::Log(utils::LOG_INFO, "Geodata")
      << "Geodata loaded: " << path << std::endl;

  return &inserted.first->second;
}

} // namespace geodata

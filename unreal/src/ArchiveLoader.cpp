#include "pch.h"

#include <unreal/Archive.h>
#include <unreal/ArchiveLoader.h>

#include "Decryptor.h"

namespace unreal {

auto ArchiveLoader::load_archive(const std::string &name) const -> Archive * {
  const auto pair = m_archives.find(name);

  if (pair != m_archives.end()) {
    return &pair->second;
  }

  utils::Log(utils::LOG_INFO, "Unreal")
      << "Loading package: " << name << std::endl;

  for (const auto &config : m_configs) {
    const auto path =
        m_root_path / config.directory / (name + "." + config.extension);

    if (!std::filesystem::exists(path)) {
      continue;
    }

    return load_and_cache_archive(name, path);
  }

  utils::Log(utils::LOG_WARN, "Unreal")
      << "Can't find package: " << name << std::endl;

  return nullptr;
}

auto ArchiveLoader::load_and_cache_archive(
    const std::string &name, const std::filesystem::path &path) const
    -> Archive * {

  std::stringstream decrypted;
  Decryptor decryptor;
  decryptor.decrypt(path, decrypted);

  if (utils::Log::level > utils::LOG_INFO) {
    dump_decrypted(path, decrypted);
  }

  const auto inserted =
      m_archives.try_emplace(name, name, std::move(decrypted), *this);
  auto *archive = &inserted.first->second;

  utils::Log(utils::LOG_INFO, "Unreal")
      << "Package loaded: " << name
      << " (file: " << archive->header.file_version
      << ", license: " << archive->header.license_version << ")" << std::endl;

  return archive;
}

void ArchiveLoader::dump_decrypted(const std::filesystem::path &path,
                                   const std::stringstream &decrypted) const {

  auto output_path = path.filename();
  output_path += ".dec";
  std::ofstream output{output_path};

  utils::Log(utils::LOG_DEBUG, "Unreal")
      << "Decrypted package: " << output_path << std::endl;

  output << decrypted.str();
}

} // namespace unreal

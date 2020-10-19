#pragma once

#include "Context.h"
#include "Texture.h"

#include <filesystem>
#include <memory>
#include <string>

namespace rendering {

class TextureLoader {
public:
  explicit TextureLoader(Context &context,
                         const std::filesystem::path &root_path);

  auto load_texture(const std::string &name) const -> std::shared_ptr<Texture>;

private:
  Context &m_context;
  const std::filesystem::path m_root_path;

  mutable std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
};

} // namespace rendering

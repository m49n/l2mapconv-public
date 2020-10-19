#include "pch.h"

#include <rendering/TextureLoader.h>

namespace rendering {

TextureLoader::TextureLoader(Context &context,
                             const std::filesystem::path &root_path)
    : m_context{context}, m_root_path{root_path} {}

auto TextureLoader::load_texture(const std::string &name) const
    -> std::shared_ptr<Texture> {

  const auto pair = m_textures.find(name);

  if (pair != m_textures.end()) {
    return pair->second;
  }

  const auto path = m_root_path / name;

  int width = 0;
  int height = 0;
  int bpp = 0;
  const auto *data = stbi_load(path.string().c_str(), &width, &height, &bpp, 4);

  if (data == nullptr) {
    return nullptr;
  }

  auto texture = std::make_shared<Texture>(m_context, Texture::Format::RGBA,
                                           width, height, data);
  m_textures[name] = texture;
  return texture;
}

} // namespace rendering

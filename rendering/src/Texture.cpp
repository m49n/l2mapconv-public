#include "pch.h"

#include <rendering/Texture.h>

namespace rendering {

Texture::Texture(Context &context, Format format, int width, int height,
                 const std::vector<std::uint8_t> &data)
    : m_context{context}, m_texture{0} {

  GL_CALL(glGenTextures(1, &m_texture));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, m_texture));

  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                          GL_LINEAR_MIPMAP_LINEAR));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
  GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

  switch (format) {
  case Format::RGBA: {
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, &data.front()));
    break;
  }
  case Format::DXT1: {
    GL_CALL(glCompressedTexImage2D(
        GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, width, height, 0,
        (width * height) / 2, &data.front()));
    break;
  }
  }

  GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
  GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

Texture::~Texture() {
  unbind();
  GL_CALL(glDeleteTextures(1, &m_texture));
}

Texture::Texture(Texture &&other) noexcept
    : m_context{other.m_context}, m_texture{other.m_texture} {

  other.m_texture = 0;
}

auto Texture::operator=(Texture &&other) noexcept -> Texture & {
  m_context = other.m_context;
  m_texture = other.m_texture;

  other.m_texture = 0;

  return *this;
}

void Texture::bind(unsigned int unit) const {
  if (m_context.texture.texture != m_texture) {
    GL_CALL(glActiveTexture(GL_TEXTURE0 + unit));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, m_texture));
    m_context.texture.texture = m_texture;
  }
}

void Texture::unbind() const {
  if (m_context.texture.texture == m_texture) {
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
    m_context.texture.texture = 0;
  }
}

} // namespace rendering

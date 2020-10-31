#pragma once

#include "Context.h"

#include <utils/NonCopyable.h>

#include <cstdint>
#include <vector>

namespace rendering {

class Texture : public utils::NonCopyable {
public:
  enum class Format {
    RGBA,
    DXT1,
    DXT3,
    DXT5,
  };

  explicit Texture(Context &context, Format format, int width, int height,
                   const unsigned char *data);

  virtual ~Texture();

  Texture(Texture &&other) noexcept;
  auto operator=(Texture &&other) noexcept -> Texture &;

  void bind(unsigned int unit) const;
  void unbind() const;

private:
  Context &m_context;
  unsigned int m_texture;
};

} // namespace rendering

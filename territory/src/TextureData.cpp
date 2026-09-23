#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <territory/TextureData.h>
namespace territory {
std::size_t expected_bytes(PixelEncoding format, int width, int height) {
  if (width <= 0 || height <= 0 || width > 32768 || height > 32768)
    throw std::invalid_argument("Invalid texture dimensions");
  const auto pixels = static_cast<std::size_t>(width) * height;
  const auto blocks =
      static_cast<std::size_t>((width + 3) / 4) * ((height + 3) / 4);
  std::size_t result = 0;
  switch (format) {
  case PixelEncoding::Rgba8:
    result = pixels * 4;
    break;
  case PixelEncoding::R8:
    result = pixels;
    break;
  case PixelEncoding::Dxt1:
    result = blocks * 8;
    break;
  case PixelEncoding::Dxt3:
  case PixelEncoding::Dxt5:
    result = blocks * 16;
    break;
  default:
    throw std::invalid_argument("Invalid pixel encoding");
  }
  if (result > 512 * 1024 * 1024)
    throw std::invalid_argument("Texture payload exceeds 512 MiB");
  return result;
}
TextureData texture_data(const unreal::Texture &texture, TextureUsage usage) {
  if (texture.mips.empty())
    throw std::runtime_error("Texture has no stored mip data");
  // Highest available resolution, not an upscaled lower mip.
  const auto &mip = *std::max_element(
      texture.mips.begin(), texture.mips.end(),
      [](const auto &a, const auto &b) {
        return static_cast<std::int64_t>(a.u_size) * a.v_size <
               static_cast<std::int64_t>(b.u_size) * b.v_size;
      });
  TextureData result;
  result.clamp_u = texture.u_clamp_mode == unreal::TC_Clamp;
  result.clamp_v = texture.v_clamp_mode == unreal::TC_Clamp;
  const auto &ref = texture.asset_reference();
  result.source = ref.package.empty() ? texture.full_name()
                                      : ref.package + '.' + ref.object_path;
  result.width = mip.u_size;
  result.height = mip.v_size;
  result.usage = usage;
  switch (texture.format) {
  case unreal::TEXF_P8: {
    if (mip.data.size() !=
        expected_bytes(PixelEncoding::R8, mip.u_size, mip.v_size))
      throw std::runtime_error("Invalid paletted mip size");
    (void)expected_bytes(PixelEncoding::Rgba8, mip.u_size, mip.v_size);
    auto palette = texture.palette.as<unreal::Palette>();
    if (!palette)
      throw std::runtime_error("Paletted texture has no readable palette");
    result.bytes = expand_p8(mip.data, palette->colors);
    return result;
  }
  case unreal::TEXF_RGBA8:
    result.encoding = PixelEncoding::Rgba8;
    break;
  case unreal::TEXF_L8:
    result.encoding = PixelEncoding::R8;
    break;
  case unreal::TEXF_DXT1:
    result.encoding = PixelEncoding::Dxt1;
    break;
  case unreal::TEXF_DXT3:
    result.encoding = PixelEncoding::Dxt3;
    break;
  case unreal::TEXF_DXT5:
    result.encoding = PixelEncoding::Dxt5;
    break;
  default:
    throw std::runtime_error("Unsupported texture format " +
                             std::to_string(texture.format));
  }
  if (mip.data.size() !=
      expected_bytes(result.encoding, mip.u_size, mip.v_size))
    throw std::runtime_error("Invalid stored mip length");
  result.bytes = mip.data;
  // UE2 RGBA8 bitmap payload is BGRA; palette entries, unlike this payload,
  // are serialized R,G,B,A and are expanded by their named fields.
  if (texture.format == unreal::TEXF_RGBA8) {
    for (std::size_t i = 0; i < result.bytes.size(); i += 4)
      std::swap(result.bytes[i], result.bytes[i + 2]);
  }
  return result;
}
std::vector<std::uint8_t> expand_p8(std::span<const std::uint8_t> indices,
                                    std::span<const unreal::Color> palette) {
  if (indices.size() > 128 * 1024 * 1024 || palette.empty() ||
      palette.size() > 256)
    throw std::runtime_error("Invalid palette expansion size");
  std::vector<std::uint8_t> result;
  result.reserve(indices.size() * 4);
  for (auto index : indices) {
    if (index >= palette.size())
      throw std::runtime_error("Palette index out of range");
    const auto &c = palette[index];
    result.insert(result.end(), {c.r, c.g, c.b, c.a});
  }
  return result;
}
float srgb_to_linear(float c) {
  return c <= .04045f ? c / 12.92f : std::pow((c + .055f) / 1.055f, 2.4f);
}
float linear_to_srgb(float c) {
  c = std::clamp(c, 0.f, 1.f);
  return c <= .0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.f / 2.4f) - .055f;
}
} // namespace territory

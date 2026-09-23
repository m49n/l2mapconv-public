#pragma once
#include <span>
#include <string>
#include <unreal/Material.h>
#include <vector>
namespace territory {
enum class TextureUsage { Color, Mask, Data };
enum class PixelEncoding { Rgba8, Dxt1, Dxt3, Dxt5, R8 };
struct TextureData {
  bool clamp_u{}, clamp_v{};
  std::string source;
  int width{}, height{};
  PixelEncoding encoding{PixelEncoding::Rgba8};
  TextureUsage usage{TextureUsage::Color};
  std::vector<std::uint8_t> bytes;
};
std::size_t expected_bytes(PixelEncoding, int width, int height);
TextureData texture_data(const unreal::Texture &, TextureUsage);
std::vector<std::uint8_t> expand_p8(std::span<const std::uint8_t>,
                                    std::span<const unreal::Color>);
float srgb_to_linear(float);
float linear_to_srgb(float);
} // namespace territory

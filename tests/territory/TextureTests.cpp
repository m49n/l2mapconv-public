#include "Fixtures.h"
#include "TestSupport.h"
#include <array>
#include <cmath>
#include <cstring>
#include <territory/TextureData.h>
int texture_tests() {
  using namespace territory;
  int failures = 0;
  std::array<unreal::Color, 256> palette{};
  palette[7] = {10, 20, 30, 40};
  const std::array<std::uint8_t, 1> indices{7};
  failures += expect(expand_p8(indices, palette) ==
                         std::vector<std::uint8_t>({10, 20, 30, 40}),
                     "use actual palette RGBA");
  failures +=
      expect(throws([&] { expand_p8(indices, std::span(palette).first(7)); }),
             "reject palette index overflow");
  failures += expect(expected_bytes(PixelEncoding::Dxt1, 2, 2) == 8,
                     "tiny DXT block rounding");
  failures += expect(expected_bytes(PixelEncoding::Dxt5, 5, 7) == 64,
                     "partial DXT block rounding");
  failures += expect(expected_bytes(PixelEncoding::Rgba8, 2, 3) == 24,
                     "RGBA byte count");
  failures += expect(throws([] { expected_bytes(PixelEncoding::Rgba8, 0, 4); }),
                     "reject zero texture dimension");
  failures +=
      expect(throws([] { expected_bytes(PixelEncoding::Rgba8, 32769, 4); }),
             "reject unreasonable texture dimensions");
  failures +=
      expect(throws([] { expected_bytes(PixelEncoding::Rgba8, 32768, 32768); }),
             "reject texture payload over 512 MiB");
  failures += expect(std::abs(srgb_to_linear(.5f) - .214041f) < .00001f,
                     "color linearization");
  failures += expect(std::abs(linear_to_srgb(.214041f) - .5f) < .00001f,
                     "output sRGB encoding");
  TestDirectory directory;
  ArchiveFixture defaults(directory.path());
  alignas(unreal::Shader) std::array<std::byte, sizeof(unreal::Shader)> storage;
  storage.fill(std::byte{0xcd});
  auto shader = std::construct_at(
      reinterpret_cast<unreal::Shader *>(storage.data()), *defaults.archive);
  failures += expect(
      shader->output_blending == unreal::OB_Normal && !shader->two_sided &&
          !shader->treat_as_two_sided,
      "omitted shader flags have deterministic normal one-sided rendering");
  std::destroy_at(shader);
  auto fixture = make_texture_fixture(directory.path());
  auto t = std::dynamic_pointer_cast<unreal::Texture>(
      fixture->archive->object_loader.load_object(unreal::Index{1}));
  failures += expect(t && t->format == unreal::TEXF_P8,
                     "omitted Format defaults to P8");
  failures += expect(t && t->palette.has_reference(),
                     "Palette property retains its reference");
  const auto decoded = texture_data(*t, TextureUsage::Mask);
  failures += expect(decoded.width == 2 && decoded.height == 2 &&
                         decoded.usage == TextureUsage::Mask &&
                         decoded.bytes == std::vector<std::uint8_t>(
                                              {10, 20, 30, 40, 0, 0, 0, 0, 0, 0,
                                               0, 0, 0, 0, 0, 0}),
                     "deserialize palette asset and expand stored indices");
  t->mips[0].data.pop_back();
  failures += expect(throws([&] { texture_data(*t, TextureUsage::Color); }),
                     "reject truncated P8 mip");
  auto dxt_fixture = make_texture_fixture(directory.path(), unreal::TEXF_DXT1);
  auto dxt = std::dynamic_pointer_cast<unreal::Texture>(
      dxt_fixture->archive->object_loader.load_object(unreal::Index{1}));
  failures +=
      expect(dxt && dxt->mips.size() == 1 && dxt->mips[0].data.size() == 8,
             "parse tiny DXT mip header not matching arbitrary bytes");
  auto short_fixture = make_texture_fixture(directory.path());
  --short_fixture->archive->export_map[0].serial_size.value;
  failures += expect(throws([&] {
                       short_fixture->archive->object_loader.load_object(
                           unreal::Index{1});
                     }),
                     "do not read beyond export into adjacent palette");
  // These malformed counts are small enough to exercise the legacy failure
  // safely.
  auto many = make_texture_fixture(directory.path(), 0, 17);
  failures +=
      expect(throws([&] {
               many->archive->object_loader.load_object(unreal::Index{1});
             }),
             "reject unreasonable mip count before reading array");
  t->mips.clear();
  failures += expect(throws([&] { texture_data(*t, TextureUsage::Color); }),
                     "reject empty mip chain");
  return failures;
}

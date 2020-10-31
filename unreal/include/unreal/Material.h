#pragma once

#include "Archive.h"
#include "Index.h"
#include "Object.h"
#include "ObjectRef.h"
#include "Primitives.h"
#include "Property.h"

#include <cstdint>
#include <vector>

namespace unreal {

class Material : public Object {
public:
  ObjectRef<Material> fallback_material;
  ObjectRef<Material> default_material;

  explicit Material(Archive &archive) : Object{archive} {}
};

class Modifier : public Material {
public:
  ObjectRef<Material> material;

  explicit Modifier(Archive &archive) : Material{archive} {}

  virtual auto set_property(const Property &property) -> bool override;
};

enum FrameBufferBlending {
  FB_Overwrite = 0,
  FB_Modulate = 1,
  FB_AlphaBlend = 2,
  FB_AlphaModulate_MightNotFogCorrectly = 3,
  FB_Translucent = 4,
  FB_Darken = 5,
  FB_Brighten = 6,
  FB_Invisible = 7,
  FB_MAX = 8,
};

class FinalBlend : public Modifier {
public:
  std::uint8_t fb_blending;
  bool z_write;
  bool z_test;
  bool alpha_test;
  bool two_sided;
  std::uint8_t alpha_ref;
  bool treat_as_two_sided;

  explicit FinalBlend(Archive &archive)
      : Modifier{archive}, fb_blending{FB_Overwrite}, z_write{true},
        z_test{true}, alpha_test{false}, two_sided{false}, alpha_ref{128},
        treat_as_two_sided{false} {}

  virtual auto set_property(const Property &property) -> bool override;
};

class RenderedMaterial : public Material {
public:
  explicit RenderedMaterial(Archive &archive) : Material{archive} {}
};

enum TextureFormat {
  TEXF_P8 = 0x00,
  TEXF_RGBA7 = 0x01,
  TEXF_RGB16 = 0x02,
  TEXF_DXT1 = 0x03,
  TEXF_RGB8 = 0x04,
  TEXF_RGBA8 = 0x05,
  TEXF_NODATA = 0x06,
  TEXF_DXT3 = 0x07,
  TEXF_DXT5 = 0x08,
  TEXF_L8 = 0x09,
  TEXF_G16 = 0x0A,
  TEXF_RRRGGGBBB = 0x0B,
  TEXF_MAX = 0xFF,
};

enum TextureClampMode {
  TC_Wrap = 0x00,
  TC_Clamp = 0x01,
};

class BitmapMaterial : public RenderedMaterial {
public:
  std::uint8_t format;
  std::uint8_t u_clamp_mode, v_clamp_mode;
  std::uint8_t u_bits, v_bits;   // # of bits in size, i.e. 8 for 256.
  std::int32_t u_size, v_size;   // Size, must be power of 2.
  std::int32_t u_clamp, v_clamp; // Clamped width, must be <= size.

  explicit BitmapMaterial(Archive &archive) : RenderedMaterial{archive} {}

  virtual auto set_property(const Property &property) -> bool override;
};

class Palette : public Object {
public:
  std::vector<Color> colors;

  explicit Palette(Archive &archive) : Object{archive} {}

  virtual void deserialize() override;
};

struct Mipmap {
  std::int32_t unknown; // ??? Pointer to data, valid only when locked.
  std::vector<std::uint8_t> data;
  std::int32_t u_size, v_size;    // Power of two tile dimensions.
  std::int8_t u_bits, v_bits;     // Power of two tile bits.

  template <typename T> auto as() const -> const T * {
    return reinterpret_cast<const T *>(data.data());
  }

  friend auto operator>>(Archive &archive, Mipmap &mipmap) -> Archive &;
};

enum LODSet {
  LODSET_None = 0,
  LODSET_World = 1,
  LODSET_PlayerSkin = 2,
  LODSET_WeaponSkin = 3,
  LODSET_Terrain = 4,
  LODSET_Interface = 5,
  LODSET_RenderMap = 6,
  LODSET_Lightmap = 7,
  LODSET_MAX = 8,
};

class Texture : public BitmapMaterial {
public:
  // Palette.
  ObjectRef<Palette> palette; // Palette if 8-bit palettized.

  // Detail texture.
  ObjectRef<Material> detail; // Detail texture to apply.
  float detail_scale;         // Detail texture scale.

  // Internal info.
  Color mip_zero;          // Overall average color of texture.
  Color max_color;         // Maximum color for normalization.
  double last_update_time; // Last time texture was locked for rendering.

  // Texture flags.
  bool masked;
  bool alpha_texture;
  bool two_sided; // Texture should be rendered two sided when placed directly
                  // on a surface.
  bool high_color_quality;   // High color quality hint.
  bool high_texture_quality; // High texture quality hint.
  bool realtime;             // Texture changes in realtime.
  bool parametric;           // Texture data need not be stored.
  bool realtime_changed;     // Changed since last render.

  // Level of detail.
  std::uint8_t lod_set;
  int normal_lod;
  int min_lod;
  int max_lod;

  // Animation.
  ObjectRef<Texture>
      animation_next; // Next texture in looped animation sequence.
  ObjectRef<Texture> animation_current; // Current animation frame.
  std::uint8_t prime_count;   // Priming total for algorithmic textures.
  std::uint8_t prime_current; // Priming current for algorithmic textures.
  float min_frame_rate;       // Minimum animation rate in fps.
  float max_frame_rate;       // Maximum animation rate in fps.
  float accumulator;          // Frame accumulator.

  // Mipmaps.
  std::vector<Mipmap> mips; // Mipmaps in native format.

  bool has_compressed;             // Compressed version included?
  TextureFormat compressed_format; // Compressed texture format.

  explicit Texture(Archive &archive)
      : BitmapMaterial{archive}, detail_scale{8.0f}, mip_zero{64, 128, 64, 0},
        max_color{255, 255, 255, 255},
        alpha_texture{false}, two_sided{false}, lod_set{LODSET_World} {}

  virtual void deserialize() override;
  virtual auto set_property(const Property &property) -> bool override;
};

enum OutputBlending {
  OB_Normal = 0,
  OB_Masked = 1,
  OB_Modulate = 2,
  OB_Translucent = 3,
  OB_Invisible = 4,
  OB_Brighten = 5,
  OB_Darken = 6,
  OB_MAX = 7,
};

class Shader : public RenderedMaterial {
public:
  std::uint8_t output_blending;
  ObjectRef<Texture, ObjectRefConstraint::Optional> diffuse;
  ObjectRef<Texture> opacity;
  ObjectRef<Texture> specular;
  ObjectRef<Texture> specular_mask;
  ObjectRef<Texture> self_illumination;
  ObjectRef<Texture> self_illumination_mask;
  ObjectRef<Texture> detail;
  float detail_scale;
  bool two_sided;
  bool wire_frame;
  bool modulate_static_lighting_2x;
  bool perform_lighting_on_specular_pass;
  bool modulate_specular_2x;
  bool treat_as_two_sided;
  bool z_write;
  bool alpha_test;
  std::uint8_t alpha_ref;

  explicit Shader(Archive &archive)
      : RenderedMaterial{archive}, detail_scale{8.0f}, z_write{true},
        alpha_test{false}, alpha_ref{128} {}

  virtual auto set_property(const Property &property) -> bool override;
}; // namespace unreal

} // namespace unreal

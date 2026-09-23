#include "pch.h"

#include <unreal/Material.h>

#include "MaterialDeserializer.h"
#include <stdexcept>

namespace unreal {
void Material::deserialize() {
  // RF_Native describes the object, not absence of tagged material properties.
  // P542 FX_E_T.WaterSurfaceShaderSet.WaterShader01 (0x040e0004) stores both
  // Diffuse and Opacity here. Keep this correction scoped to materials.
  if ((flags & RF_HasStack) != 0) archive >> state_frame;
  const auto properties=archive.property_extractor.extract_properties();
  for(const auto& property:properties)set_property(property);
}
auto Material::set_property(const Property& p) -> bool {
  if (p.name == "FallbackMaterial") { fallback_material.from_property(p, archive); return true; }
  if (p.name == "DefaultMaterial") { default_material.from_property(p, archive); return true; }
  return Object::set_property(p);
}
auto ConstantColor::set_property(const Property& p) -> bool {
  if (p.name == "Color") { color = p.color_value; return true; }
  return Material::set_property(p);
}

auto Combiner::set_property(const Property &property) -> bool {
  if (property.name == "Mask") { mask.from_property(property, archive); return true; }
  if (property.name == "CombineOperation") { combine_operation = property.uint8_t_value; return true; }
  if (property.name == "AlphaOperation") { alpha_operation = property.uint8_t_value; return true; }
  if (property.name == "InvertMask") { invert_mask = property.bool_value(); return true; }
  if (property.name == "Modulate2X") { modulate_2x = property.bool_value(); return true; }
  if (property.name == "Modulate4X") { modulate_4x = property.bool_value(); return true; }
  if (Material::set_property(property)) {
    return true;
  }

  if (property.name == "Material1") {
    material1.from_property(property, archive);
    return true;
  }

  if (property.name == "Material2") {
    material2.from_property(property, archive);
    return true;
  }

  return false;
}

auto Modifier::set_property(const Property &property) -> bool {
  if (property.name == "UScale") { u_scale = property.float_value; return true; }
  if (property.name == "VScale") { v_scale = property.float_value; return true; }
  if (property.name == "UOffset") { u_offset = property.float_value; return true; }
  if (property.name == "VOffset") { v_offset = property.float_value; return true; }
  if (property.name == "PanRate") { pan_rate = property.float_value; return true; }
  if (property.name == "PanDirection") { pan_direction = property.rotator_value; return true; }
  if (property.name == "Rotation") { rotation = property.rotator_value; return true; }
  if (property.name == "TexRotationType") { rotation_type = property.uint8_t_value; return true; }
  if (property.name == "TexCoordSource") { tex_coord_source = property.uint8_t_value; return true; }
  if (Material::set_property(property)) {
    return true;
  }

  if (property.name == "Material") {
    material.from_property(property, archive);
    return true;
  }

  return false;
}

auto FinalBlend::set_property(const Property &property) -> bool {
  if (Modifier::set_property(property)) {
    return true;
  }

  if (property.name == "FrameBufferBlending") {
    fb_blending = static_cast<FrameBufferBlending>(property.uint8_t_value);
    return true;
  }

  if (property.name == "ZWrite") {
    z_write = property.bool_value();
    return true;
  }

  if (property.name == "ZTest") {
    z_test = property.bool_value();
    return true;
  }

  if (property.name == "AlphaTest") {
    alpha_test = property.bool_value();
    return true;
  }

  if (property.name == "TwoSided") {
    two_sided = property.bool_value();
    return true;
  }

  if (property.name == "AlphaRef") {
    alpha_ref = property.uint8_t_value;
    return true;
  }

  if (property.name == "TreatAsTwoSided") {
    treat_as_two_sided = property.bool_value();
    return true;
  }

  return false;
}

auto BitmapMaterial::set_property(const Property &property) -> bool {
  if (RenderedMaterial::set_property(property)) {
    return true;
  }

  if (property.name == "Format") {
    format = static_cast<TextureFormat>(property.uint8_t_value);
    return true;
  }

  if (property.name == "UBits") {
    u_bits = property.uint8_t_value;
    return true;
  }

  if (property.name == "VBits") {
    v_bits = property.uint8_t_value;
    return true;
  }

  if (property.name == "USize") {
    u_size = property.int32_t_value;
    return true;
  }

  if (property.name == "VSize") {
    v_size = property.int32_t_value;
    return true;
  }

  if (property.name == "UClamp") {
    u_clamp = property.int32_t_value;
    return true;
  }

  if (property.name == "VClamp") {
    v_clamp = property.int32_t_value;
    return true;
  }

  return false;
}

void Palette::deserialize() {
  Object::deserialize();
  Index count{}; archive >> count;
  if (count.value < 0 || count.value > 256) throw std::runtime_error("Invalid palette color count");
  archive.require_bytes(static_cast<std::size_t>(count.value) * 4);
  colors.resize(static_cast<std::size_t>(count.value));
  for (auto& color : colors) archive >> color;
}

auto operator>>(Archive &archive, Mipmap &mipmap) -> Archive & {
  Index count{};
  archive >> mipmap.unknown >> count;
  if (count.value < 0 || count.value > 512 * 1024 * 1024) throw std::runtime_error("Invalid texture mip byte count");
  archive.require_bytes(static_cast<std::size_t>(count.value) + 10);
  mipmap.data.resize(static_cast<std::size_t>(count.value));
  auto& input = static_cast<std::istream&>(archive);
  input.read(reinterpret_cast<char*>(mipmap.data.data()), count.value);
  if (!input) throw std::runtime_error("Truncated texture mip data");
  if (mipmap.unknown != 0 && mipmap.unknown != input.tellg()) throw std::runtime_error("Invalid texture mip data-end offset");
  archive >> mipmap.u_size >> mipmap.v_size >> mipmap.u_bits >> mipmap.v_bits;
  if (mipmap.u_size <= 0 || mipmap.v_size <= 0 || mipmap.u_size > 32768 || mipmap.v_size > 32768)
    throw std::runtime_error("Invalid texture mip dimensions");
  return archive;
}

void Texture::deserialize() {
  BitmapMaterial::deserialize();

  MaterialDeserializer deserializer{};
  deserializer.deserialize(archive);

  Index count{}; archive >> count;
  if (count.value < 0 || count.value > 16) throw std::runtime_error("Invalid texture mip count");
  mips.clear(); mips.reserve(static_cast<std::size_t>(count.value));
  std::size_t total = 0;
  for (int i = 0; i < count.value; ++i) {
    Mipmap mip{}; archive >> mip;
    total += mip.data.size();
    if (total > 512 * 1024 * 1024) throw std::runtime_error("Texture payload exceeds 512 MiB");
    const auto pixels = static_cast<std::size_t>(mip.u_size) * mip.v_size;
    const auto blocks = static_cast<std::size_t>((mip.u_size + 3) / 4) * ((mip.v_size + 3) / 4);
    std::size_t expected = 0;
    switch (format) {
      case TEXF_P8: case TEXF_L8: expected = pixels; break;
      case TEXF_G16: expected = pixels * 2; break;
      case TEXF_RGBA8: expected = pixels * 4; break;
      case TEXF_DXT1: expected = blocks * 8; break;
      case TEXF_DXT3: case TEXF_DXT5: expected = blocks * 16; break;
      default: break; // Unsupported formats remain identifiable for reporting.
    }
    if (expected && mip.data.size() != expected) throw std::runtime_error("Texture mip length does not match dimensions");
    mips.push_back(std::move(mip));
  }
}

auto Texture::set_property(const Property &property) -> bool {
  if (BitmapMaterial::set_property(property)) {
    return true;
  }
  if (property.name == "Palette") { palette.from_property(property, archive); return true; }
  if (property.name == "bMasked") { masked = property.bool_value(); return true; }

  if (property.name == "bAlphaTexture") {
    alpha_texture = property.bool_value();
    return true;
  }

  if (property.name == "bTwoSided") {
    two_sided = property.bool_value();
    return true;
  }

  return false;
}

auto Shader::set_property(const Property &property) -> bool {
  const std::pair<const char*, MaterialReference*> references[] = {
    {"Opacity", &opacity}, {"Specular", &specular}, {"SpecularityMask", &specular_mask},
    {"SelfIllumination", &self_illumination}, {"SelfIlluminationMask", &self_illumination_mask}, {"Detail", &detail}};
  for (const auto& [name, ref] : references) if (property.name == name) { ref->from_property(property, archive); return true; }
  if (property.name == "ZWrite") { z_write = property.bool_value(); return true; }
  if (property.name == "Wireframe") { wire_frame = property.bool_value(); return true; }
  if (property.name == "DetailScale") { detail_scale = property.float_value; return true; }
  if (property.name == "ModulateStaticLighting2X") { modulate_static_lighting_2x = property.bool_value(); return true; }
  if (property.name == "PerformLightingOnSpecularPass") { perform_lighting_on_specular_pass = property.bool_value(); return true; }
  if (property.name == "ModulateSpecular2X") { modulate_specular_2x = property.bool_value(); return true; }
  if (RenderedMaterial::set_property(property)) {
    return true;
  }

  if (property.name == "Diffuse") {
    diffuse.from_property(property, archive);
    return true;
  }

  if (property.name == "OutputBlending") {
    output_blending = static_cast<OutputBlending>(property.uint8_t_value);
    return true;
  }

  if (property.name == "AlphaTest") {
    alpha_test = property.bool_value();
    return true;
  }

  if (property.name == "AlphaRef") {
    alpha_ref = property.uint8_t_value;
    return true;
  }

  if (property.name == "TreatAsTwoSided") {
    treat_as_two_sided = property.bool_value();
    return true;
  }

  if (property.name == "TwoSided") {
    two_sided = property.bool_value();
    return true;
  }

  return false;
}

} // namespace unreal

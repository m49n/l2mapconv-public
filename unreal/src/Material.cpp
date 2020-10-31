#include "pch.h"

#include <unreal/Material.h>

#include "MaterialDeserializer.h"

namespace unreal {

auto Modifier::set_property(const Property &property) -> bool {
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
  archive >> colors;
}

auto operator>>(Archive &archive, Mipmap &mipmap) -> Archive & {
  archive >> mipmap.unknown >> mipmap.data >> mipmap.u_size >> mipmap.v_size >>
      mipmap.u_bits >> mipmap.v_bits;
  return archive;
}

void Texture::deserialize() {
  BitmapMaterial::deserialize();

  MaterialDeserializer deserializer;
  deserializer.deserialize(archive);

  archive >> mips;
}

auto Texture::set_property(const Property &property) -> bool {
  if (BitmapMaterial::set_property(property)) {
    return true;
  }

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

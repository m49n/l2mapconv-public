// Copy-pasted from https://github.com/gildor2/UEViewer
// (c) Gildor

#include "pch.h"

#include "MaterialDeserializer.h"

namespace unreal {

struct MaterialStageProperty {
  std::string unk1;
  std::vector<std::string> unk2;

  friend auto operator>>(Archive &archive, MaterialStageProperty &property)
      -> Archive & {

    archive >> property.unk1 >> property.unk2;
    return archive;
  }
};

struct ShaderProperty {
  // possibly, MaterialInfo, TextureTranform, TwoPassRenderState, AlphaRef
  std::uint8_t b1, b2;
  std::uint8_t b3[5], b4[5], b5[5], b6[5], b7[5], b8[5];

  // possibly, SrcBlend, DestBlend, OverriddenFogColor
  std::uint32_t i1[5], i2[5], i3[5];

  // nested structure
  // possibly, int FC_Color1, FC_Color2 (strange byte order)
  std::uint8_t be[8];
  // possibly, float FC_FadePeriod, FC_FadePhase, FC_ColorFadeType
  std::uint32_t ie[3];

  // stages
  std::vector<MaterialStageProperty> stages;

  friend auto operator>>(Archive &archive, ShaderProperty &property)
      -> Archive & {

    const auto file_version = archive.header.file_version;

    archive >> property.b1 >> property.b2;

    if (file_version < 129) {
      archive >> property.b3[0] >> property.b4[0] >> property.i1[0] >>
          property.i2[0] >> property.i3[0];
    } else if (file_version == 129) {
      for (auto i = 0; i < 5; ++i) {
        archive >> property.b3[i] >> property.b4[i];
        archive >> property.i1[i] >> property.i2[i] >> property.i3[i];
      }
    } else // if (file_version >= 130)
    {
      for (auto i = 0; i < 5; ++i) {
        archive >> property.b3[i] >> property.b4[i] >> property.b5[i] >>
            property.b6[i] >> property.b7[i] >> property.b8[i];
        archive >> property.i2[i] >> property.i3[i];
      }
    }

    for (auto i = 0; i < 8; ++i) {
      archive >> property.be[i];
    }

    for (auto i = 0; i < 3; ++i) {
      archive >> property.ie[i];
    }

    archive >> property.stages;
    return archive;
  }
};

void MaterialDeserializer::deserialize(Archive &archive) {
  const auto license_version = archive.header.license_version;
  const auto file_version = archive.header.file_version;

  if (file_version >= 123 && license_version >= 16 && license_version < 37) {
    std::uint32_t unk1;
    archive >> unk1; // simply drop obsolete variable (int Reserved ?)
  }

  if (file_version >= 123 && license_version >= 30 && license_version < 37) {
    // some function
    {
      std::uint8_t material_info, texture_transform, max_sampler_num,
          max_texmat_num, max_pass_num, two_pass_render_state, alpha_ref;

      if (license_version >= 33 && license_version < 36) {
        archive >> material_info;
      }

      archive >> texture_transform >> max_sampler_num >> max_texmat_num >>
          max_pass_num >> two_pass_render_state >> alpha_ref;

      std::uint32_t src_blend, dest_blend, overriden_fog_color;
      archive >> src_blend >> dest_blend >> overriden_fog_color;

      // serialize matTexMatrix[16] (strange code)
      for (auto i = 0; i < 8; ++i) {
        std::uint8_t b1;
        archive >> b1;

        if (license_version < 36) {
          std::uint8_t b2;
          archive >> b2;
        }

        for (auto j = 0; j < 126; ++j) {
          // really, 1Kb of floats and ints ...
          std::uint8_t b3;
          archive >> b3;
        }
      }

      // another nested function - serialize FC_* variables
      {
        std::uint8_t c[8]; // union with "int FC_Color1, FC_Color2" (strange
                           // byte order)
        archive >> c[2] >> c[1] >> c[0] >> c[3] >> c[6] >> c[5] >> c[4] >> c[7];

        std::uint32_t fc_fade_period, fc_fade_phase,
            fc_color_fade_type; // really, floats?
        archive >> fc_fade_period >> fc_fade_phase >> fc_color_fade_type;
      } // end of nested function

      for (auto i = 0; i < 16; ++i) {
        std::string str_tex; // strTex[16]
        archive >> str_tex;
      }
    } // end of function

    std::string shader_code;
    archive >> shader_code;
  }

  if (file_version >= 123 && license_version >= 37) {
    // ShaderProperty + ShaderCode
    ShaderProperty shader_property{};
    std::string shader_code;
    archive >> shader_property >> shader_code;
  }

  if (file_version >= 123 && license_version >= 31) {
    std::uint16_t ver1,
        ver2; // "int  MaterialCodeVersion" serialized as 2 words
    archive >> ver1 >> ver2;
  }
}

} // namespace unreal

#pragma once
#include "TextureData.h"
#include <array>
#include <glm/glm.hpp>
#include <optional>
namespace territory {
enum class Op {
  Constant,
  Sample,
  Multiply,
  Add,
  Subtract,
  Lerp,
  AlphaReplace,
  VertexColor
};
struct Node {
  Op op{Op::Constant};
  glm::vec4 value{1.f};
  std::vector<std::uint32_t> inputs;
  std::optional<std::uint32_t> texture;
  glm::mat3 uv{1.f};
  int uv_channel{};
};
enum class Blend {
  Opaque,
  Masked,
  Alpha,
  Modulate,
  Translucent,
  Add,
  Invisible
};
struct MaterialReferenceStep {
  std::string source, class_name, parent, state;
  TextureUsage usage{TextureUsage::Color};
};
struct RenderMaterial {
  std::string source;
  std::string class_name;
  std::vector<MaterialReferenceStep> reference_chain;
  std::vector<Node> nodes;
  std::uint32_t root{};
  Blend blend{Blend::Opaque};
  float alpha_ref{128.f / 255.f};
  bool depth_write{true}, depth_test{true}, two_sided{}, alpha_test{};
  bool unlit{};
  // False when a recovered material graph substitutes unknown alpha/coverage.
  // Such geometry must not cast a solid polygon shadow.
  bool shadow_coverage_reliable{true};
};
struct MaterialLibrary {
  std::vector<TextureData> textures;
  std::vector<RenderMaterial> materials;
};
glm::vec4 evaluate_node(Op, glm::vec4 a, glm::vec4 b, float mask);
// Returns each unavailable sampled channel while replacing its node with a
// neutral color; the shadow caster is marked unsafe if any were replaced.
std::vector<int> neutralize_missing_uv_samples(
    RenderMaterial &, const std::array<bool, 4> &available_channels);
// GLSL function body: validated node_N statements and return. No client text is
// emitted.
std::string material_expression(const RenderMaterial &);
} // namespace territory

#include "LiveVisualShaders.h"

#include <set>

namespace live_shaders {

auto vertex() -> const std::string & {
  static const std::string source = R"glsl(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv0;
layout(location=3) in vec2 uv1;
layout(location=4) in vec2 uv2;
layout(location=5) in vec2 uv3;
layout(location=6) in vec4 color;
uniform mat4 model;
uniform mat4 view_projection;
uniform mat3 normal_matrix;
uniform mat4 world_to_uv;
uniform vec2 region_origin;
uniform vec2 region_size;
uniform bool terrain;
out vec2 v_uv[4];
out vec3 v_normal;
out vec4 v_color;
out vec2 v_mask;
out vec3 v_world;
void main() {
  vec3 world = (model * vec4(position, 1.0)).xyz;
  v_uv[0] = terrain ? (world_to_uv * vec4(world, 1.0)).xy : uv0;
  v_uv[1] = uv1; v_uv[2] = uv2; v_uv[3] = uv3;
  v_normal = normal_matrix * normal;
  v_color = color;
  v_mask = (world.xy - region_origin) / region_size;
  v_world = world;
  gl_Position = view_projection * vec4(world, 1.0);
})glsl";
  return source;
}

auto color_fragment(const territory::RenderMaterial &material,
                    bool terrain_mask, bool shadows) -> std::string {
  using territory::Op;
  std::set<std::uint32_t> samples;
  std::string source = R"glsl(#version 330 core
in vec2 v_uv[4];
in vec3 v_normal;
in vec4 v_color;
in vec2 v_mask;
in vec3 v_world;
uniform bool show_textures;
uniform bool alpha_test;
uniform float alpha_ref;
uniform bool unlit;
uniform vec3 flat_color;
out vec4 fragment;
)glsl";
  for (const auto &node : material.nodes) {
    if (node.op == Op::Sample && node.texture) samples.insert(*node.texture);
  }
  for (auto id : samples)
    source += "uniform sampler2D tex_" + std::to_string(id) + ";\n";
  for (std::size_t i = 0; i < material.nodes.size(); ++i)
    if (material.nodes[i].op == Op::Sample)
      source += "uniform mat3 uv_" + std::to_string(i) + ";\n";
  if (terrain_mask) source += "uniform sampler2D terrain_mask;\n";
  if (shadows)
    source += "uniform sampler2DShadow shadow_map;\n"
              "uniform mat4 light_matrix;\n"
              "uniform vec3 sun_direction;\n"
              "uniform float shadow_texel;\n";
  source += "vec4 material() {\n" + territory::material_expression(material) +
            "}\n";
  source += R"glsl(void main() {
  vec4 color = material();
  if (alpha_test && color.a < alpha_ref) discard;
)glsl";
  if (terrain_mask)
    source += "color.a *= texture(terrain_mask, v_mask).r;\n"
              "if (color.a < 0.004) discard;\n";
  source += R"glsl(
  if (!show_textures) color.rgb = flat_color;
  float shade = 1.0;
  if (!unlit) {
    vec3 n = length(v_normal) > 0.0001 ? normalize(v_normal) : vec3(0,0,1);
    if (!gl_FrontFacing) n = -n;
    shade = 0.72 + 0.28 * max(dot(n, normalize(vec3(-0.35,-0.50,1.0))), 0.0);
  }
)glsl";
  if (shadows)
    source += R"glsl(
  vec4 light = light_matrix * vec4(v_world, 1.0);
  vec3 projected = light.xyz / light.w * 0.5 + 0.5;
  float visibility = 1.0;
  if (all(greaterThanEqual(projected, vec3(0.0))) &&
      all(lessThanEqual(projected, vec3(1.0)))) {
    vec3 n = length(v_normal) > 0.0001 ? normalize(v_normal) : vec3(0,0,1);
    if (!gl_FrontFacing) n = -n;
    float bias = clamp(1.5 * shadow_texel *
                       (1.0 + 2.0 * (1.0 - max(dot(n, sun_direction), 0.0))),
                       0.0002, 0.01);
    visibility = 0.0;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        visibility += texture(shadow_map,
                              vec3(projected.xy + vec2(dx, dy) * shadow_texel,
                                   projected.z - bias));
    visibility /= 9.0;
  }
  color.rgb *= mix(0.58, 1.0, visibility);
)glsl";
  source += R"glsl(
  fragment = vec4(color.rgb * shade, color.a);
})glsl";
  return source;
}

auto depth_fragment(const territory::RenderMaterial &material) -> std::string {
  using territory::Op;
  std::set<std::uint32_t> samples;
  std::string source = R"glsl(#version 330 core
in vec2 v_uv[4];
in vec4 v_color;
uniform float alpha_ref;
)glsl";
  for (const auto &node : material.nodes)
    if (node.op == Op::Sample && node.texture) samples.insert(*node.texture);
  for (auto id : samples)
    source += "uniform sampler2D tex_" + std::to_string(id) + ";\n";
  for (std::size_t i = 0; i < material.nodes.size(); ++i)
    if (material.nodes[i].op == Op::Sample)
      source += "uniform mat3 uv_" + std::to_string(i) + ";\n";
  source += "vec4 material() {\n" + territory::material_expression(material) +
            "}\nvoid main() { if (material().a < alpha_ref) discard; }\n";
  return source;
}
} // namespace live_shaders

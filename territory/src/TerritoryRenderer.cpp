#include "GLResources.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <glm/gtc/type_ptr.hpp>
#include <limits>
#include <map>
#include <set>
#include <territory/GeometricNormals.h>
#include <territory/ShadowProjection.h>
#include <territory/TerritoryRenderer.h>
#include <territory/TileLayout.h>
#include <thread>
namespace territory {
namespace {
using gl::Handle;
using gl::Kind;
const std::string vertex_source = R"glsl(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv0;
layout(location=3) in vec2 uv1;
layout(location=4) in vec2 uv2;
layout(location=5) in vec2 uv3;
layout(location=6) in vec4 color;
uniform mat4 model;
uniform mat3 normal_matrix;
uniform vec4 tile_bounds;
uniform vec2 z_bounds;
uniform vec2 region_size;
uniform mat4 world_to_uv;
uniform bool terrain;
out vec2 v_uv[4];out vec3 v_normal;out vec4 v_color;out vec2 v_mask;
void main() {
  vec3 local=(model*vec4(position,1.0)).xyz;
  v_uv[0]=terrain ? (world_to_uv*vec4(local,1.0)).xy : uv0;
  v_uv[1]=uv1;v_uv[2]=uv2;v_uv[3]=uv3;
  v_mask=local.xy/region_size;
  v_normal=normal_matrix*normal;v_color=color;
  gl_Position=vec4(2.0*(local.x-tile_bounds.x)/tile_bounds.z-1.0,
    1.0-2.0*(local.y-tile_bounds.y)/tile_bounds.w,
    1.0-2.0*(local.z-z_bounds.x)/(z_bounds.y-z_bounds.x),1.0);
})glsl";
const std::string shadow_color_vertex_source = R"glsl(#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 uv0;
layout(location=3) in vec2 uv1;
layout(location=4) in vec2 uv2;
layout(location=5) in vec2 uv3;
layout(location=6) in vec4 color;
uniform mat4 model;
uniform mat3 normal_matrix;
uniform vec4 tile_bounds;
uniform vec2 z_bounds;
uniform vec2 region_size;
uniform mat4 world_to_uv;
uniform bool terrain;
out vec2 v_uv[4];out vec3 v_normal;out vec4 v_color;out vec2 v_mask;
out vec3 v_world;
void main() {
  vec3 local=(model*vec4(position,1.0)).xyz;
  v_uv[0]=terrain ? (world_to_uv*vec4(local,1.0)).xy : uv0;
  v_uv[1]=uv1;v_uv[2]=uv2;v_uv[3]=uv3;
  v_mask=local.xy/region_size;
  v_world=local;
  v_normal=normal_matrix*normal;v_color=color;
  gl_Position=vec4(2.0*(local.x-tile_bounds.x)/tile_bounds.z-1.0,
    1.0-2.0*(local.y-tile_bounds.y)/tile_bounds.w,
    1.0-2.0*(local.z-z_bounds.x)/(z_bounds.y-z_bounds.x),1.0);
})glsl";
const std::string shadow_vertex_source = R"glsl(#version 330 core
layout(location=0) in vec3 position;
layout(location=2) in vec2 uv0;
layout(location=3) in vec2 uv1;
layout(location=4) in vec2 uv2;
layout(location=5) in vec2 uv3;
layout(location=6) in vec4 color;
uniform mat4 model;
uniform mat4 shadow_matrix;
uniform mat4 world_to_uv;
uniform vec2 region_size;
uniform bool terrain;
out vec2 v_uv[4];out vec4 v_color;out vec2 v_mask;
void main() {
  vec3 local=(model*vec4(position,1.0)).xyz;
  v_uv[0]=terrain ? (world_to_uv*vec4(local,1.0)).xy : uv0;
  v_uv[1]=uv1;v_uv[2]=uv2;v_uv[3]=uv3;
  v_color=color;
  v_mask=local.xy/region_size;
  gl_Position=shadow_matrix*vec4(local,1.0);
})glsl";
std::set<std::uint32_t> samples(const RenderMaterial &m) {
  std::set<std::uint32_t> result;
  for (auto &n : m.nodes)
    if (n.texture)
      result.insert(*n.texture);
  return result;
}
std::string fragment_source(const RenderMaterial &m, bool mask,
                            bool shadows) {
  std::string s = R"glsl(#version 330 core
in vec2 v_uv[4];in vec3 v_normal;in vec4 v_color;in vec2 v_mask;
uniform bool alpha_test;uniform float alpha_ref;uniform bool unlit;
uniform bool show_textures;uniform vec3 flat_color;
out vec4 fragment;
)glsl";
  if (mask)
    s += "uniform sampler2D terrain_mask;\n";
  for (auto i : samples(m))
    s += "uniform sampler2D tex_" + std::to_string(i) + ";\n";
  for (std::size_t i = 0; i < m.nodes.size(); ++i)
    if (m.nodes[i].op == Op::Sample)
      s += "uniform mat3 uv_" + std::to_string(i) + ";\n";
  if (shadows)
    s += R"glsl(in vec3 v_world;
uniform sampler2DShadow shadow_map;
uniform mat4 shadow_matrix;
uniform vec3 sun_direction;
uniform float shadow_texel;
)glsl";
  s += "vec4 material() {\n" + material_expression(m) + "}\n";
  if (shadows) {
    s += R"glsl(void main() {
  vec4 color=material();
  if(alpha_test && color.a<alpha_ref)discard;
  vec3 n=length(v_normal)>0.0001?normalize(v_normal):vec3(0,0,1);
  if(!gl_FrontFacing)n=-n;
  float shade=1.0;
  if(!unlit)shade=0.72+0.28*max(dot(n,sun_direction),0.0);
)glsl";
  } else {
  s += R"glsl(void main() {
  vec4 color=material();
  if(alpha_test && color.a<alpha_ref)discard;
  float shade=1.0;
  if(!unlit) {
    vec3 n=length(v_normal)>0.0001?normalize(v_normal):vec3(0,0,1);
    if(!gl_FrontFacing)n=-n;
    shade=0.72+0.28*max(dot(n,normalize(vec3(-0.35,-0.50,1.0))),0.0);
  }
  )glsl";
  }
  if (mask)
    s += "color.a=texture(terrain_mask,v_mask).r;\n";
  s += "if(!show_textures)color.rgb=flat_color;\n";
  if (shadows)
    s += R"glsl(vec4 light=shadow_matrix*vec4(v_world,1.0);
  vec3 projected=light.xyz/light.w*0.5+0.5;
  float visibility=1.0;
  if(all(greaterThanEqual(projected,vec3(0.0))) &&
     all(lessThanEqual(projected,vec3(1.0)))) {
    visibility=0.0;
    float slope=1.0-max(dot(n,sun_direction),0.0);
    float bias=clamp(1.5*shadow_texel*(1.0+2.0*slope),0.0002,0.01);
    for(int dy=-1;dy<=1;++dy)
      for(int dx=-1;dx<=1;++dx)
        visibility+=texture(shadow_map,vec3(projected.xy+
          vec2(dx,dy)*shadow_texel,projected.z-bias));
    visibility/=9.0;
  }
  color.rgb*=mix(0.58,1.0,visibility);
)glsl";
  s += R"glsl(
  fragment=vec4(color.rgb*shade,color.a);
})glsl";
  return s;
}
std::string shadow_fragment_source(const RenderMaterial &m) {
  std::string s = R"glsl(#version 330 core
in vec2 v_uv[4];in vec4 v_color;in vec2 v_mask;
uniform float alpha_ref;
)glsl";
  for (auto i : samples(m))
    s += "uniform sampler2D tex_" + std::to_string(i) + ";\n";
  for (std::size_t i = 0; i < m.nodes.size(); ++i)
    if (m.nodes[i].op == Op::Sample)
      s += "uniform mat3 uv_" + std::to_string(i) + ";\n";
  s += "vec4 material() {\n" + material_expression(m) + "}\n";
  s += "void main(){if(material().a<alpha_ref)discard;}\n";
  return s;
}
GLint uniform(GLuint p, const char *name) {
  return glGetUniformLocation(p, name);
}
void matrix(GLuint p, const char *name, const glm::mat4 &m) {
  glUniformMatrix4fv(uniform(p, name), 1, GL_FALSE, glm::value_ptr(m));
}
bool transparent(Blend b) {
  return b == Blend::Alpha || b == Blend::Modulate || b == Blend::Translucent ||
         b == Blend::Add;
}
struct Mesh {
  Handle vao{Kind::VertexArray}, vbo{Kind::Buffer}, ibo{Kind::Buffer};
  glm::dvec3 origin{};
  explicit Mesh(const VisualMesh &mesh) {
    if (mesh.vertices.empty())
      return;
    origin = mesh.vertices.front().position;
    auto vertices = mesh.vertices;
    for (auto &v : vertices)
      v.position = glm::vec3(glm::dvec3(v.position) - origin);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VisualVertex),
                 vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh.indices.size() * sizeof(std::uint32_t),
                 mesh.indices.data(), GL_STATIC_DRAW);
    auto attribute = [](GLuint id, int size, std::size_t offset) {
      glEnableVertexAttribArray(id);
      glVertexAttribPointer(id, size, GL_FLOAT, GL_FALSE, sizeof(VisualVertex),
                            reinterpret_cast<const void *>(offset));
    };
    attribute(0, 3, offsetof(VisualVertex, position));
    attribute(1, 3, offsetof(VisualVertex, normal));
    for (int i = 0; i < 4; ++i)
      attribute(2 + i, 2, offsetof(VisualVertex, uv) + i * sizeof(glm::vec2));
    attribute(6, 4, offsetof(VisualVertex, color));
    gl::check("mesh upload");
  }
};
Handle texture(const TextureData &data) {
  if (data.bytes.size() !=
      expected_bytes(data.encoding, data.width, data.height))
    throw std::runtime_error("Texture byte count changed before upload");
  GLint limit = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limit);
  if (data.width > limit || data.height > limit)
    throw std::runtime_error("GPU texture size limit: " + data.source);
  Handle result(Kind::Texture);
  glBindTexture(GL_TEXTURE_2D, result);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  const bool srgb = data.usage == TextureUsage::Color;
  if (data.encoding == PixelEncoding::Rgba8)
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8,
                 data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data.bytes.data());
  else if (data.encoding == PixelEncoding::R8) {
    if (srgb) {
      std::vector<std::uint8_t> rgb;
      rgb.reserve(data.bytes.size() * 3);
      for (auto v : data.bytes)
        rgb.insert(rgb.end(), {v, v, v});
      glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, data.width, data.height, 0,
                   GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    } else {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, data.width, data.height, 0, GL_RED,
                   GL_UNSIGNED_BYTE, data.bytes.data());
      const GLint swizzle[] = {GL_RED, GL_RED, GL_RED, GL_RED};
      glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    }
  } else {
    if (!GLEW_EXT_texture_compression_s3tc)
      throw std::runtime_error("GPU does not support S3TC texture compression");
    GLenum format = 0;
    switch (data.encoding) {
    case PixelEncoding::Dxt1:
      format = srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
                    : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
      break;
    case PixelEncoding::Dxt3:
      format = srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
                    : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
      break;
    case PixelEncoding::Dxt5:
      format = srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                    : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
      break;
    default:
      throw std::runtime_error("Unsupported GPU encoding");
    }
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height, 0,
                           static_cast<GLsizei>(data.bytes.size()),
                           data.bytes.data());
  }
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                  data.clamp_u ? GL_CLAMP_TO_EDGE : GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                  data.clamp_v ? GL_CLAMP_TO_EDGE : GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  GL_LINEAR_MIPMAP_LINEAR);
  glGenerateMipmap(GL_TEXTURE_2D);
  if (GLEW_EXT_texture_filter_anisotropic) {
    GLfloat max = 1;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                    std::min(8.f, max));
  }
  gl::check("texture upload/mipmap generation");
  return result;
}
struct Framebuffers {
  Handle draw{Kind::Framebuffer}, resolve{Kind::Framebuffer},
      color{Kind::Renderbuffer}, depth{Kind::Renderbuffer},
      resolved_color{Kind::Renderbuffer};
  Framebuffers(int width, int height, int samples) {
    glBindFramebuffer(GL_FRAMEBUFFER, draw);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_SRGB8_ALPHA8,
                                     width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, color);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples,
                                     GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      throw std::runtime_error("Incomplete multisample framebuffer");
    glBindFramebuffer(GL_FRAMEBUFFER, resolve);
    glBindRenderbuffer(GL_RENDERBUFFER, resolved_color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8_ALPHA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, resolved_color);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      throw std::runtime_error("Incomplete resolve framebuffer");
    gl::check("framebuffer allocation");
  }
};
struct ShadowFramebuffer {
  Handle framebuffer{Kind::Framebuffer}, depth{Kind::Texture};
  explicit ShadowFramebuffer(int size) {
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const GLfloat border[] = {1.f, 1.f, 1.f, 1.f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                    GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           depth, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      throw std::runtime_error("Shadow depth framebuffer unavailable");
    gl::check("shadow framebuffer allocation");
  }
};
struct PreparedDraw {
  const Draw *draw{};
  std::size_t mesh{}, first_index{}, index_count{};
  bool mirrored{};
  glm::mat4 model{1};
  glm::mat3 normals{1};
  glm::dvec3 min{}, max{};
};
} // namespace
struct TerritoryRenderer::Impl {
  GLFWwindow *window{};
  std::thread::id owner = std::this_thread::get_id();
  Impl() {
    if (!glfwInit())
      throw std::runtime_error("GLFW initialization failed");
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window =
        glfwCreateWindow(32, 32, "Territory render worker", nullptr, nullptr);
    if (!window) {
      glfwTerminate();
      throw std::runtime_error("OpenGL 3.3 offscreen context unavailable");
    }
    glfwMakeContextCurrent(window);
    glewExperimental = GL_TRUE;
    auto error = glewInit();
    while (glGetError() != GL_NO_ERROR) {
    }
    if (error != GLEW_OK) {
      glfwDestroyWindow(window);
      window = nullptr;
      glfwTerminate();
      throw std::runtime_error("GLEW initialization failed");
    }
  }
  ~Impl() {
    if (window) {
      glfwMakeContextCurrent(window);
      glfwDestroyWindow(window);
    }
    glfwTerminate();
  }
};
TerritoryRenderer::TerritoryRenderer() : impl(std::make_unique<Impl>()) {}
TerritoryRenderer::~TerritoryRenderer() = default;
RasterInfo TerritoryRenderer::render(const VisualScene &scene,
                                     const RasterSettings &settings,
                                     const Cancel &cancel,
                                     const TileProgress &progress,
                                     const Rows &rows) {
  check_cancel(cancel);
  if (std::this_thread::get_id() != impl->owner)
    throw std::runtime_error("Renderer used from a different thread");
  if (!rows)
    throw std::invalid_argument("Raster row sink is required");
  const auto tiles =
      tile_layout(scene.bounds, settings.pixels, settings.tile_size);
  validate_scene(scene);
  if (settings.samples != 1 && settings.samples != 2 && settings.samples != 4 &&
      settings.samples != 8)
    throw std::invalid_argument("Unsupported MSAA request");
  glfwMakeContextCurrent(impl->window);
  GLint max_samples = 1, max_renderbuffer = 0, max_units = 0;
  glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
  glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer);
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_units);
  if (settings.max_texture_units > 0)
    max_units = std::min(max_units, settings.max_texture_units);
  if (settings.shadows &&
      (!std::isfinite(settings.sun_azimuth_deg) ||
       settings.sun_azimuth_deg < 0 || settings.sun_azimuth_deg > 360 ||
       !std::isfinite(settings.sun_elevation_deg) ||
       settings.sun_elevation_deg < 15 || settings.sun_elevation_deg > 80))
    throw std::invalid_argument("Invalid shadow sun angles");
  if (settings.tile_size > max_renderbuffer)
    throw std::runtime_error("GPU framebuffer tile size limit");
  const int sample_count = std::min(settings.samples, max_samples);
  RasterInfo info{
      std::string(reinterpret_cast<const char *>(glGetString(GL_RENDERER))) +
          " / " + reinterpret_cast<const char *>(glGetString(GL_VERSION)),
      sample_count};
  std::vector<Handle> textures;
  textures.reserve(scene.library.textures.size());
  for (auto &data : scene.library.textures) {
    check_cancel(cancel);
    textures.push_back(texture(data));
  }
  std::vector<Mesh> meshes;
  meshes.reserve(scene.meshes.size());
  for (auto &mesh : scene.meshes) {
    check_cancel(cancel);
    meshes.emplace_back(mesh);
  }
  std::map<std::string, Handle> programs;
  auto get_program = [&](const RenderMaterial &m, bool mask) {
    auto fs = fragment_source(m, mask, settings.shadows);
    auto found = programs.find(fs);
    if (found != programs.end())
      return static_cast<GLuint>(found->second);
    check_cancel(cancel);
    auto p = gl::program(settings.shadows ? shadow_color_vertex_source
                                          : vertex_source,
                         fs);
    auto id = static_cast<GLuint>(p);
    programs.emplace(std::move(fs), std::move(p));
    return id;
  };
  struct PreparedMaterial {
    RenderMaterial material;
    GLuint program{};
  };
  using Variant = std::pair<std::size_t, bool>;
  std::map<Variant, PreparedMaterial> material_programs;
  std::map<Variant, std::set<std::string>> used_materials;
  for (const auto &draw : scene.draws)
    if (draw.index_count && (!draw.water || settings.water) &&
        scene.library.materials[draw.material].blend != Blend::Invisible)
      used_materials[{draw.material, false}].insert(draw.source);
  if (scene.terrain_mesh)
    for (std::size_t i = 0; i < scene.terrain_layers.size(); ++i)
      used_materials[{scene.terrain_layers[i].material, true}].insert(
          "terrain:layer:" + std::to_string(i));
  for (const auto &[key, surfaces] : used_materials) {
    auto material = scene.library.materials[key.first];
    const auto required = samples(material).size() + std::size_t(key.second);
    if (settings.shadows && required + 1 > static_cast<std::size_t>(max_units))
      throw std::runtime_error(
          "GPU sampler limit cannot reserve a unit for shadow map: " +
          material.source);
    if (required > static_cast<std::size_t>(max_units)) {
      info.issues.push_back(
          {IssueKind::Unsupported,
           material.source,
           "GPU sampler limit: material requires " + std::to_string(required) +
               " slots including terrain mask; available " +
               std::to_string(max_units) + "; using neutral material",
           {surfaces.begin(), surfaces.end()}});
      material.nodes.clear();
      Node n;
      n.value = {.5f, .5f, .5f, 1};
      material.nodes.push_back(n);
      material.root = 0;
    }
    const auto program = get_program(material, key.second);
    material_programs.emplace(key,
                              PreparedMaterial{std::move(material), program});
  }
  RenderMaterial neutral;
  neutral.source = "terrain neutral";
  Node constant;
  constant.value = {.35f, .35f, .35f, 1};
  neutral.nodes.push_back(constant);
  neutral.two_sided = true;
  const auto neutral_program = get_program(neutral, false);
  const glm::dvec3 region(scene.bounds.min_x, scene.bounds.min_y, 0);
  auto prepare = [&](const Draw &d, const VisualMesh &mesh, std::size_t gpu_mesh,
                     bool geometric_normals = false) {
    PreparedDraw p;
    p.draw = &d;
    p.mesh = gpu_mesh;
    p.first_index = geometric_normals ? 0 : d.first_index;
    p.index_count = geometric_normals ? mesh.indices.size() : d.index_count;
    p.mirrored = glm::determinant(glm::dmat3(d.transform)) < 0;
    auto transform = glm::dmat4(d.transform);
    auto origin = transform * glm::dvec4(meshes[gpu_mesh].origin, 1.0);
    transform[3] = origin - glm::dvec4(region, 0);
    p.model = glm::mat4(transform);
    if (!geometric_normals)
      p.normals = glm::transpose(glm::inverse(glm::mat3(d.transform)));
    p.min = glm::dvec3(std::numeric_limits<double>::max());
    p.max = -p.min;
    for (std::size_t i = p.first_index; i < p.first_index + p.index_count; ++i) {
      auto world =
          glm::dvec3(glm::dmat4(d.transform) *
                     glm::dvec4(mesh.vertices[mesh.indices[i]].position, 1));
      p.min = glm::min(p.min, world);
      p.max = glm::max(p.max, world);
    }
    return p;
  };
  std::vector<PreparedDraw> opaque, alpha;
  for (auto &draw : scene.draws) {
    check_cancel(cancel);
    if (!draw.index_count || (draw.water && !settings.water))
      continue;
    auto blend = scene.library.materials[draw.material].blend;
    if (blend == Blend::Invisible)
      continue;
    // P542 16_24.StaticMeshActor64 has a serialized DrawScale of zero.
    // Point collapses can be skipped without examining individual triangles.
    if (glm::mat3(draw.transform) == glm::mat3(0.f)) {
      info.issues.push_back({IssueKind::Simplified,
                             draw.source,
                             "Zero-scale object collapsed to a point; no "
                             "visible surface to render",
                             {draw.source}});
      continue;
    }
    const auto determinant = glm::determinant(glm::mat3(draw.transform));
    PreparedDraw p;
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12f) {
      auto rebuilt =
          rebuild_geometric_normals(scene.meshes[draw.mesh], draw, cancel);
      info.issues.push_back({IssueKind::Simplified, draw.source,
                            "Singular or near-singular transform: retained " +
                                std::to_string(rebuilt.mesh.indices.size() / 3) +
                                " triangles with geometric face normals; skipped " +
                                std::to_string(rebuilt.skipped_triangles) +
                                " zero-area triangles",
                            {draw.source}});
      if (rebuilt.mesh.indices.empty())
        continue;
      const auto index = meshes.size();
      meshes.emplace_back(rebuilt.mesh);
      p = prepare(draw, rebuilt.mesh, index, true);
    } else {
      p = prepare(draw, scene.meshes[draw.mesh], draw.mesh);
    }
    (transparent(blend) ? alpha : opaque).push_back(std::move(p));
  }
  std::stable_sort(alpha.begin(), alpha.end(),
                   [](const auto &a, const auto &b) {
                     return a.min.z + a.max.z < b.min.z + b.max.z;
                   });
  std::optional<Draw> terrain_draw;
  std::optional<PreparedDraw> terrain_prepared;
  if (scene.terrain_mesh) {
    terrain_draw = Draw{*scene.terrain_mesh,
                        0,
                        0,
                        scene.meshes[*scene.terrain_mesh].indices.size(),
                        glm::mat4(1),
                        false,
                        "terrain"};
    terrain_prepared = prepare(*terrain_draw, scene.meshes[*scene.terrain_mesh],
                               *scene.terrain_mesh);
  }
  const int shadow_unit = max_units - 1;
  ShadowProjection shadow_projection;
  std::optional<ShadowFramebuffer> shadow_framebuffer;
  std::size_t done = 0;
  const std::size_t progress_total = tiles.size() + std::size_t(settings.shadows);
  if (settings.shadows) {
    const int shadow_size = std::min(settings.pixels, 4096);
    GLint max_texture_size = 0, max_viewport[2]{};
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport);
    if (settings.max_shadow_texture_size > 0)
      max_texture_size =
          std::min(max_texture_size, settings.max_shadow_texture_size);
    if (shadow_size > max_texture_size || shadow_size > max_viewport[0] ||
        shadow_size > max_viewport[1])
      throw std::runtime_error("GPU shadow depth-map size limit: requested " +
                               std::to_string(shadow_size));
    Bounds coverage = scene.bounds;
    auto include_caster = [&](const PreparedDraw &p) {
      coverage.min_x = std::min(coverage.min_x, p.min.x);
      coverage.min_y = std::min(coverage.min_y, p.min.y);
      coverage.max_x = std::max(coverage.max_x, p.max.x);
      coverage.max_y = std::max(coverage.max_y, p.max.y);
      coverage.min_z = std::min(coverage.min_z, p.min.z);
      coverage.max_z = std::max(coverage.max_z, p.max.z);
    };
    if (terrain_prepared)
      include_caster(*terrain_prepared);
    for (const auto &p : opaque)
      if (!p.draw->water &&
          scene.library.materials[p.draw->material].shadow_coverage_reliable)
        include_caster(p);
    shadow_projection = make_shadow_projection(
        scene.bounds, coverage, settings.sun_azimuth_deg,
        settings.sun_elevation_deg, shadow_size);
    shadow_framebuffer.emplace(shadow_size);
    info.shadow_map_size = shadow_size;
    Handle opaque_shadow_program = gl::program(
        shadow_vertex_source, "#version 330 core\nvoid main(){}\n");
    std::map<std::size_t, Handle> masked_shadow_programs;
    std::vector<std::optional<std::size_t>> omitted_caster_issues(
        scene.library.materials.size());
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_framebuffer->framebuffer);
    glViewport(0, 0, shadow_size, shadow_size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.25f, 2.f);
    glClearDepth(1.0);
    glClear(GL_DEPTH_BUFFER_BIT);
    auto cast = [&](const PreparedDraw &p, const RenderMaterial &material,
                    std::size_t material_id) {
      check_cancel(cancel);
      if (p.draw->water)
        return;
      if (!material.shadow_coverage_reliable) {
        auto &index = omitted_caster_issues.at(material_id);
        if (!index) {
          index = info.issues.size();
          info.issues.push_back(
              {IssueKind::Simplified, material.source,
               "Shadow caster omitted: material coverage cannot be evaluated "
               "reliably",
               {}});
        }
        auto &surfaces = info.issues[*index].surfaces;
        if (std::find(surfaces.begin(), surfaces.end(), p.draw->source) ==
            surfaces.end())
          surfaces.push_back(p.draw->source);
        return;
      }
      const bool masked = material.alpha_test || material.blend == Blend::Masked;
      GLuint program = opaque_shadow_program;
      if (masked) {
        auto found = masked_shadow_programs.find(material_id);
        if (found == masked_shadow_programs.end()) {
          auto shader = gl::program(shadow_vertex_source,
                                    shadow_fragment_source(material));
          found = masked_shadow_programs
                      .emplace(material_id, std::move(shader))
                      .first;
        }
        program = found->second;
      }
      glUseProgram(program);
      matrix(program, "model", p.model);
      matrix(program, "shadow_matrix",
             shadow_projection.relative_world_to_clip);
      glUniform1i(uniform(program, "terrain"), false);
      glUniform2f(uniform(program, "region_size"),
                  float(scene.bounds.max_x - scene.bounds.min_x),
                  float(scene.bounds.max_y - scene.bounds.min_y));
      matrix(program, "world_to_uv", glm::mat4(1.f));
      if (masked) {
        glUniform1f(uniform(program, "alpha_ref"), material.alpha_ref);
        int unit = 0;
        for (auto id : samples(material)) {
          glActiveTexture(GL_TEXTURE0 + unit);
          glBindTexture(GL_TEXTURE_2D, textures[id]);
          glUniform1i(uniform(program, ("tex_" + std::to_string(id)).c_str()),
                      unit++);
        }
        for (std::size_t i = 0; i < material.nodes.size(); ++i)
          if (material.nodes[i].op == Op::Sample)
            glUniformMatrix3fv(
                uniform(program, ("uv_" + std::to_string(i)).c_str()), 1,
                GL_FALSE, glm::value_ptr(material.nodes[i].uv));
      }
      glFrontFace(p.mirrored ? GL_CW : GL_CCW);
      if (material.two_sided)
        glDisable(GL_CULL_FACE);
      else {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
      }
      glBindVertexArray(meshes[p.mesh].vao);
      glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(p.index_count),
                     GL_UNSIGNED_INT,
                     reinterpret_cast<const void *>(p.first_index *
                                                    sizeof(std::uint32_t)));
    };
    if (terrain_prepared)
      cast(*terrain_prepared, neutral, scene.library.materials.size());
    for (const auto &p : opaque)
      cast(p, scene.library.materials[p.draw->material], p.draw->material);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gl::check("shadow depth pass");
    if (progress)
      progress(++done, progress_total);
    else
      ++done;
  }
  const int n = settings.pixels;
  Framebuffers fb(settings.tile_size, settings.tile_size, sample_count);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glDisable(GL_DITHER);
  glEnable(GL_MULTISAMPLE);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  std::vector<std::uint8_t> band, readback;
  int band_y = -1, band_height = 0;
  auto draw = [&](const PreparedDraw &p, const RenderMaterial &m,
                  GLuint program, const Tile &tile, const TerrainLayer *layer,
                  bool terrain_base) {
    if (p.max.x < tile.world.min_x || p.min.x > tile.world.max_x ||
        p.max.y < tile.world.min_y || p.min.y > tile.world.max_y)
      return;
    glUseProgram(program);
    matrix(program, "model", p.model);
    glUniformMatrix3fv(uniform(program, "normal_matrix"), 1, GL_FALSE,
                       glm::value_ptr(p.normals));
    glUniform4f(uniform(program, "tile_bounds"),
                float(tile.world.min_x - region.x),
                float(tile.world.min_y - region.y),
                float(tile.world.max_x - tile.world.min_x),
                float(tile.world.max_y - tile.world.min_y));
    glUniform2f(uniform(program, "z_bounds"), float(tile.world.min_z),
                float(tile.world.max_z));
    glUniform2f(uniform(program, "region_size"),
                float(scene.bounds.max_x - scene.bounds.min_x),
                float(scene.bounds.max_y - scene.bounds.min_y));
    glUniform1i(uniform(program, "terrain"), layer || terrain_base);
    glUniform1i(uniform(program, "alpha_test"),
                m.alpha_test || m.blend == Blend::Masked);
    glUniform1f(uniform(program, "alpha_ref"), m.alpha_ref);
    glUniform1i(uniform(program, "unlit"), m.unlit);
    if (settings.shadows) {
      matrix(program, "shadow_matrix",
             shadow_projection.relative_world_to_clip);
      glUniform3fv(uniform(program, "sun_direction"), 1,
                   glm::value_ptr(shadow_projection.surface_to_sun));
      glUniform1f(uniform(program, "shadow_texel"),
                  1.f / shadow_projection.size);
      glActiveTexture(GL_TEXTURE0 + shadow_unit);
      glBindTexture(GL_TEXTURE_2D, shadow_framebuffer->depth);
      glUniform1i(uniform(program, "shadow_map"), shadow_unit);
    }
    glUniform1i(uniform(program, "show_textures"), settings.textures);
    const glm::vec3 flat_color = terrain_base
                                     ? glm::vec3(.35f)
                                     : layer ? glm::vec3(.55f)
                                             : p.draw->water
                                                   ? glm::vec3(.28f, .38f, .47f)
                                                   : glm::vec3(.62f);
    glUniform3f(uniform(program, "flat_color"), flat_color.x, flat_color.y,
                flat_color.z);
    auto world_uv = layer ? glm::dmat4(layer->world_to_uv) : glm::dmat4(1);
    world_uv[3] += world_uv * glm::dvec4(region, 0);
    matrix(program, "world_to_uv", glm::mat4(world_uv));
    int unit = 0;
    for (auto id : samples(m)) {
      if (unit >= max_units - int(layer != nullptr) - int(settings.shadows))
        throw std::logic_error("Unprepared GPU sampler overflow: " + m.source);
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_2D, textures[id]);
      glUniform1i(uniform(program, ("tex_" + std::to_string(id)).c_str()),
                  unit++);
    }
    for (std::size_t i = 0; i < m.nodes.size(); ++i)
      if (m.nodes[i].op == Op::Sample)
        glUniformMatrix3fv(
            uniform(program, ("uv_" + std::to_string(i)).c_str()), 1, GL_FALSE,
            glm::value_ptr(m.nodes[i].uv));
    if (layer) {
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_2D, textures[layer->mask_texture]);
      glUniform1i(uniform(program, "terrain_mask"), unit);
    }
    if (m.depth_test || layer || terrain_base)
      glEnable(GL_DEPTH_TEST);
    else
      glDisable(GL_DEPTH_TEST);
    glDepthFunc(layer ? GL_LEQUAL : GL_LESS);
    glDepthMask(layer ? GL_FALSE : GLboolean(m.depth_write));
    glFrontFace(p.mirrored ? GL_CCW : GL_CW);
    if (m.two_sided || layer || terrain_base)
      glDisable(GL_CULL_FACE);
    else {
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
    }
    if (layer || transparent(m.blend)) {
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      if (layer || m.blend == Blend::Alpha)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      else if (m.blend == Blend::Modulate)
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
      else if (m.blend == Blend::Translucent)
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
      else
        glBlendFunc(GL_ONE, GL_ONE);
    } else
      glDisable(GL_BLEND);
    glBindVertexArray(meshes[p.mesh].vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(p.index_count),
                   GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(p.first_index *
                                                  sizeof(std::uint32_t)));
  };
  for (const auto &tile : tiles) {
    check_cancel(cancel);
    if (tile.y != band_y) {
      band_y = tile.y;
      band_height = tile.height;
      band.assign(static_cast<std::size_t>(n) * band_height * 3, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fb.draw);
    glViewport(0, 0, tile.width, tile.height);
    glDepthMask(GL_TRUE);
    glClearColor(.18f, .18f, .18f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (terrain_prepared) {
      draw(*terrain_prepared, neutral, neutral_program, tile, nullptr, true);
      for (auto &layer : scene.terrain_layers) {
        check_cancel(cancel);
        const auto &m = material_programs.at({layer.material, true});
        draw(*terrain_prepared, m.material, m.program, tile, &layer, false);
      }
    }
    for (auto &p : opaque) {
      check_cancel(cancel);
      const auto &m = material_programs.at({p.draw->material, false});
      draw(p, m.material, m.program, tile, nullptr, false);
    }
    for (auto &p : alpha) {
      check_cancel(cancel);
      const auto &m = material_programs.at({p.draw->material, false});
      draw(p, m.material, m.program, tile, nullptr, false);
    }
    gl::check("territory draw");
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.draw);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb.resolve);
    glBlitFramebuffer(0, 0, tile.width, tile.height, 0, 0, tile.width,
                      tile.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb.resolve);
    readback.resize(static_cast<std::size_t>(tile.width) * tile.height * 3);
    glReadPixels(0, 0, tile.width, tile.height, GL_RGB, GL_UNSIGNED_BYTE,
                 readback.data());
    gl::check("tile resolve/readback");
    for (int y = 0; y < tile.height; ++y)
      std::copy_n(readback.data() +
                      static_cast<std::size_t>(tile.height - 1 - y) *
                          tile.width * 3,
                  tile.width * 3,
                  band.data() + (static_cast<std::size_t>(y) * n + tile.x) * 3);
    if (progress)
      progress(++done, progress_total);
    else
      ++done;
    if (tile.x + tile.width == n) {
      check_cancel(cancel);
      rows(band_y, n, band_height, band);
    }
  }
  glBindVertexArray(0);
  glUseProgram(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  gl::check("territory render complete");
  return info;
}
} // namespace territory

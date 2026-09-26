#include "LiveVisualRenderer.h"

#include "LiveVisualShaders.h"

#include <GL/glew.h>
#include <geometry/Box.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <territory/ShadowProjection.h>
#include <territory/GeometricNormals.h>
#include <map>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

auto shader(GLenum type, const std::string &source) -> GLuint {
  const auto id = glCreateShader(type);
  const char *contents = source.c_str();
  glShaderSource(id, 1, &contents, nullptr);
  glCompileShader(id);
  GLint ok{};
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    std::array<char, 2048> log{};
    glGetShaderInfoLog(id, static_cast<GLsizei>(log.size()), nullptr,
                       log.data());
    glDeleteShader(id);
    throw std::runtime_error{"Live GLSL compile: " + std::string(log.data())};
  }
  return id;
}

auto program(const std::string &vertex, const std::string &fragment) -> GLuint {
  const auto vs = shader(GL_VERTEX_SHADER, vertex);
  GLuint fs{};
  try {
    fs = shader(GL_FRAGMENT_SHADER, fragment);
  } catch (...) {
    glDeleteShader(vs);
    throw;
  }
  const auto id = glCreateProgram();
  glAttachShader(id, vs);
  glAttachShader(id, fs);
  glLinkProgram(id);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint ok{};
  glGetProgramiv(id, GL_LINK_STATUS, &ok);
  if (!ok) {
    std::array<char, 2048> log{};
    glGetProgramInfoLog(id, static_cast<GLsizei>(log.size()), nullptr,
                        log.data());
    glDeleteProgram(id);
    throw std::runtime_error{"Live GLSL link: " + std::string(log.data())};
  }
  return id;
}

auto uniform(GLuint program_id, const char *name) -> GLint {
  return glGetUniformLocation(program_id, name);
}

void matrix(GLint location, const glm::mat4 &value) {
  glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
}

auto samples(const territory::RenderMaterial &material)
    -> std::set<std::uint32_t> {
  std::set<std::uint32_t> result;
  for (const auto &node : material.nodes)
    if (node.op == territory::Op::Sample && node.texture)
      result.insert(*node.texture);
  return result;
}

auto transparent(territory::Blend blend) -> bool {
  using territory::Blend;
  return blend == Blend::Alpha || blend == Blend::Modulate ||
         blend == Blend::Translucent || blend == Blend::Add;
}

// The legacy renderer caches GL bindings outside the driver. Every live pass
// must leave both the real bindings and that cache in agreement, including
// when an upload or draw throws.
struct LegacyBindingScope {
  rendering::Context &context;
  ~LegacyBindingScope() {
    glDisable(GL_POLYGON_OFFSET_FILL);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glFrontFace(GL_CCW);
    glUseProgram(0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    context.shader.program = 0;
    context.mesh.vao = 0;
    context.texture.texture = 0;
  }
};

struct GpuMesh {
  GLuint vao{}, vbo{}, ibo{};
  glm::vec3 origin{};
  explicit GpuMesh(const territory::VisualMesh &mesh) {
    if (mesh.vertices.empty()) return;
    origin = mesh.vertices.front().position;
    auto vertices = mesh.vertices;
    for (auto &vertex : vertices) vertex.position -= origin;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(vertices[0])),
                 vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);
    const auto attribute = [](GLuint index, GLint count, std::size_t offset) {
      glEnableVertexAttribArray(index);
      glVertexAttribPointer(index, count, GL_FLOAT, GL_FALSE,
                            sizeof(territory::VisualVertex),
                            reinterpret_cast<const void *>(offset));
    };
    attribute(0, 3, offsetof(territory::VisualVertex, position));
    attribute(1, 3, offsetof(territory::VisualVertex, normal));
    for (int channel = 0; channel < 4; ++channel)
      attribute(2 + channel, 2,
                offsetof(territory::VisualVertex, uv) +
                    channel * sizeof(glm::vec2));
    attribute(6, 4, offsetof(territory::VisualVertex, color));
    glBindVertexArray(0);
  }
  GpuMesh(const GpuMesh &) = delete;
  ~GpuMesh() {
    if (ibo) glDeleteBuffers(1, &ibo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
  }
};

struct GpuTexture {
  GLuint id{};
  explicit GpuTexture(const territory::TextureData &data) {
    if (data.width <= 0 || data.height <= 0 ||
        data.bytes.size() != territory::expected_bytes(data.encoding, data.width,
                                                        data.height))
      throw std::runtime_error{"Invalid live texture: " + data.source};
    GLint max_size{};
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    if (data.width > max_size || data.height > max_size)
      throw std::runtime_error{"Live texture exceeds GPU limit: " + data.source};
    if (data.encoding != territory::PixelEncoding::Rgba8 &&
        data.encoding != territory::PixelEncoding::R8 &&
        !GLEW_EXT_texture_compression_s3tc)
      throw std::runtime_error{"S3TC is unavailable"};
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    const bool srgb = data.usage == territory::TextureUsage::Color;
    using territory::PixelEncoding;
    if (data.encoding == PixelEncoding::Rgba8) {
      glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8,
                   data.width, data.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                   data.bytes.data());
    } else if (data.encoding == PixelEncoding::R8) {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, data.width, data.height, 0,
                   GL_RED, GL_UNSIGNED_BYTE, data.bytes.data());
      const GLint swizzle[] = {GL_RED, GL_RED, GL_RED, GL_RED};
      glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    } else {
      GLenum format{};
      if (data.encoding == PixelEncoding::Dxt1)
        format = srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
                      : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
      else if (data.encoding == PixelEncoding::Dxt3)
        format = srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
                      : GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
      else if (data.encoding == PixelEncoding::Dxt5)
        format = srgb ? GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
                      : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
      glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, data.width, data.height,
                             0, static_cast<GLsizei>(data.bytes.size()),
                             data.bytes.data());
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    data.clamp_u ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                    data.clamp_v ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_2D);
  }
  ~GpuTexture() { if (id) glDeleteTextures(1, &id); }
  GpuTexture(const GpuTexture &) = delete;
};

struct TextureKey {
  std::string source;
  int width{}, height{};
  territory::PixelEncoding encoding{};
  territory::TextureUsage usage{};
  bool clamp_u{}, clamp_v{};
  std::uint64_t bytes_hash{};
  auto operator<=>(const TextureKey &) const = default;
};

auto texture_key(const territory::TextureData &data) -> TextureKey {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const auto byte : data.bytes) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  return {data.source, data.width, data.height, data.encoding, data.usage,
          data.clamp_u, data.clamp_v, hash};
}

struct Item {
  std::size_t mesh{}, material{}, first{}, count{};
  GLuint program{};
  mutable GLuint shadow_program{};
  glm::mat4 model{1}, world_to_uv{1};
  glm::mat3 normal_matrix{1};
  glm::vec3 flat{.62f};
  bool terrain{}, layer{}, csg{}, water{}, mirrored{};
  std::size_t mask_texture{};
  geometry::Box bounds;
};

struct ProgramUniforms {
  GLint model{}, view_projection{}, normal_matrix{}, world_to_uv{};
  GLint region_origin{}, region_size{}, terrain{}, alpha_test{}, alpha_ref{};
  GLint unlit{}, show_textures{}, flat_color{}, terrain_mask{};
  GLint light_matrix{}, sun_direction{}, shadow_texel{}, shadow_map{};
  std::vector<std::pair<std::uint32_t, GLint>> samplers;
  std::vector<std::pair<std::size_t, GLint>> uv_matrices;
  ProgramUniforms(GLuint id, const territory::RenderMaterial &material) {
    model = uniform(id, "model");
    view_projection = uniform(id, "view_projection");
    normal_matrix = uniform(id, "normal_matrix");
    world_to_uv = uniform(id, "world_to_uv");
    region_origin = uniform(id, "region_origin");
    region_size = uniform(id, "region_size");
    terrain = uniform(id, "terrain");
    alpha_test = uniform(id, "alpha_test");
    alpha_ref = uniform(id, "alpha_ref");
    unlit = uniform(id, "unlit");
    show_textures = uniform(id, "show_textures");
    flat_color = uniform(id, "flat_color");
    terrain_mask = uniform(id, "terrain_mask");
    light_matrix = uniform(id, "light_matrix");
    sun_direction = uniform(id, "sun_direction");
    shadow_texel = uniform(id, "shadow_texel");
    shadow_map = uniform(id, "shadow_map");
    for (auto texture_id : samples(material))
      samplers.emplace_back(
          texture_id,
          uniform(id, ("tex_" + std::to_string(texture_id)).c_str()));
    for (std::size_t node = 0; node < material.nodes.size(); ++node)
      if (material.nodes[node].op == territory::Op::Sample)
        uv_matrices.emplace_back(
            node, uniform(id, ("uv_" + std::to_string(node)).c_str()));
  }
};

struct Group {
  territory::Bounds bounds;
  int fallback_textures{};
  int fallback_materials{};
  int singular_recovered{};
  int collapsed_skipped{};
  std::vector<std::unique_ptr<GpuMesh>> meshes;
  std::vector<std::shared_ptr<GpuTexture>> textures;
  std::vector<territory::RenderMaterial> materials;
  std::vector<Item> base, opaque, alpha;
  std::map<std::string, GLuint> programs;
  std::map<GLuint, ProgramUniforms> uniforms;
  std::map<std::pair<std::size_t, bool>, GLuint> color_programs_by_material;
  std::map<std::pair<std::size_t, bool>, GLuint> shadow_programs_by_material;
  std::map<std::size_t, GLuint> depth_programs_by_material;
  ~Group() {
    for (const auto &[source, id] : programs) {
      (void)source;
      glDeleteProgram(id);
    }
  }
};

struct ShadowResources {
  GLuint framebuffer{}, depth{};
  int size{};
  explicit ShadowResources(int requested, int test_limit) : size{requested} {
    GLint max_texture{}, viewport[2]{};
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, viewport);
    if (test_limit > 0) max_texture = std::min(max_texture, test_limit);
    if (requested > max_texture || requested > viewport[0] ||
        requested > viewport[1])
      throw std::runtime_error{"GPU shadow depth-map size limit: requested " +
                               std::to_string(requested)};
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                    GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr float border[] = {1.f, 1.f, 1.f, 1.f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           depth, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      glDeleteTextures(1, &depth);
      glDeleteFramebuffers(1, &framebuffer);
      depth = framebuffer = 0;
      throw std::runtime_error{"Shadow depth framebuffer unavailable"};
    }
  }
  ~ShadowResources() {
    if (depth) glDeleteTextures(1, &depth);
    if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
  }
};

} // namespace

namespace {
auto visible(const LiveSceneSettings &settings, const Item &item) -> bool {
  if (item.terrain) return settings.terrain;
  return item.csg ? settings.csg : settings.static_meshes;
}
} // namespace

struct LiveVisualRenderer::Impl {
  rendering::Context &context;
  int max_shadow_texture_size_for_test{};
  std::map<rendering::SceneGroupId, std::unique_ptr<Group>> groups;
  std::unique_ptr<ShadowResources> shadow;
  std::map<TextureKey, std::weak_ptr<GpuTexture>> texture_cache;

  explicit Impl(rendering::Context &value, int limit)
      : context{value}, max_shadow_texture_size_for_test{limit} {}
  auto get_texture(const territory::TextureData &data)
      -> std::shared_ptr<GpuTexture> {
    const auto key = texture_key(data);
    auto found = texture_cache.find(key);
    if (found != texture_cache.end())
      if (auto existing = found->second.lock()) return existing;
    auto uploaded = std::make_shared<GpuTexture>(data);
    texture_cache.insert_or_assign(key, uploaded);
    return uploaded;
  }
  void prune_textures() {
    for (auto it = texture_cache.begin(); it != texture_cache.end();) {
      if (it->second.expired()) it = texture_cache.erase(it);
      else ++it;
    }
  }
  auto get_program(Group &group, const territory::RenderMaterial &material,
                   bool mask,
                   bool shadows = false)
      -> GLuint {
    const auto fragment = live_shaders::color_fragment(material, mask, shadows);
    auto found = group.programs.find(fragment);
    if (found != group.programs.end()) return found->second;
    const auto id = program(live_shaders::vertex(), fragment);
    group.programs.emplace(fragment, id);
    group.uniforms.emplace(id, ProgramUniforms{id, material});
    return id;
  }
  auto get_depth_program(Group &group,
                         const territory::RenderMaterial &material,
                         bool masked) -> GLuint {
    const auto fragment = masked ? live_shaders::depth_fragment(material)
                                 : std::string{"#version 330 core\nvoid main(){}\n"};
    auto found = group.programs.find(fragment);
    if (found != group.programs.end()) return found->second;
    const auto id = program(live_shaders::vertex(), fragment);
    group.programs.emplace(fragment, id);
    group.uniforms.emplace(id, ProgramUniforms{id, material});
    return id;
  }
};

LiveVisualRenderer::LiveVisualRenderer(rendering::Context &context, int limit)
    : m_impl{std::make_unique<Impl>(context, limit)} {}

LiveVisualRenderer::~LiveVisualRenderer() = default;

void LiveVisualRenderer::upload(rendering::SceneGroupId group_id,
                                const territory::VisualScene &scene) {
  LegacyBindingScope binding_scope{m_impl->context};
  territory::validate_scene(scene);
  auto group = std::make_unique<Group>();
  group->bounds = scene.bounds;
  group->materials = scene.library.materials;
  GLint max_samplers{};
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_samplers);
  // Keep one unit for the shadow map and one for a terrain layer mask. A
  // single unsupported graph must not make the entire Detail square vanish.
  for (auto &material : group->materials) {
    if (static_cast<int>(samples(material).size()) + 2 <= max_samplers)
      continue;
    territory::Node neutral;
    neutral.value = {.5f, .5f, .5f, 1.f};
    material.nodes = {neutral};
    material.root = 0;
    material.alpha_test = false;
    material.unlit = true;
    material.shadow_coverage_reliable = false;
    ++group->fallback_materials;
  }
  group->meshes.reserve(scene.meshes.size());
  std::vector<geometry::Box> mesh_bounds;
  mesh_bounds.reserve(scene.meshes.size());
  for (const auto &mesh : scene.meshes) {
    group->meshes.push_back(std::make_unique<GpuMesh>(mesh));
    geometry::Box bounds;
    for (const auto &vertex : mesh.vertices) bounds += vertex.position;
    mesh_bounds.push_back(bounds);
  }
  group->textures.reserve(scene.library.textures.size());
  for (std::size_t index = 0; index < scene.library.textures.size(); ++index) {
    try {
      group->textures.push_back(
          m_impl->get_texture(scene.library.textures[index]));
    } catch (const std::exception &) {
      territory::TextureData neutral;
      neutral.source = "live neutral fallback";
      neutral.width = neutral.height = 1;
      neutral.bytes = {128, 128, 128, 255};
      group->textures.push_back(m_impl->get_texture(neutral));
      ++group->fallback_textures;
      for (auto &material : group->materials)
        for (const auto &node : material.nodes)
          if (node.texture && *node.texture == index)
            material.shadow_coverage_reliable = false;
    }
  }
  const auto prepare = [&](const territory::Draw &draw, std::size_t material,
                           bool terrain, bool layer, std::size_t mask,
                           const glm::mat4 &world_to_uv, glm::vec3 flat,
                           bool geometric_normals) {
    Item item;
    item.mesh = draw.mesh;
    item.material = material;
    item.first = draw.first_index;
    item.count = draw.index_count;
    item.model = draw.transform *
                 glm::translate(glm::mat4{1}, group->meshes.at(draw.mesh)->origin);
    item.world_to_uv = world_to_uv;
    item.normal_matrix = geometric_normals
                             ? glm::mat3{1}
                             : glm::transpose(glm::inverse(glm::mat3(draw.transform)));
    item.flat = flat;
    item.terrain = terrain;
    item.layer = layer;
    item.csg = !terrain && draw.source.find(".BSP:") != std::string::npos;
    item.water = draw.water;
    item.mirrored = glm::determinant(glm::mat3(draw.transform)) < 0;
    item.mask_texture = mask;
    item.bounds = geometry::Box{mesh_bounds.at(draw.mesh), draw.transform};
    const auto key = std::pair{material, layer};
    auto cached = group->color_programs_by_material.find(key);
    if (cached == group->color_programs_by_material.end()) {
      const auto program_id = m_impl->get_program(
          *group, group->materials.at(material), layer);
      cached = group->color_programs_by_material.emplace(key, program_id).first;
    }
    item.program = cached->second;
    return item;
  };
  if (scene.terrain_mesh) {
    territory::RenderMaterial neutral;
    neutral.source = "live terrain base";
    neutral.nodes.push_back({});
    neutral.nodes.front().value = {.35f, .35f, .35f, 1.f};
    neutral.two_sided = true;
    const auto neutral_id = group->materials.size();
    group->materials.push_back(neutral);
    const territory::Draw base{*scene.terrain_mesh, neutral_id, 0,
                                scene.meshes[*scene.terrain_mesh].indices.size(),
                                glm::mat4{1}, false, "terrain"};
    group->base.push_back(prepare(base, neutral_id, true, false, 0,
                                   glm::mat4{1}, {.35f, .35f, .35f}, false));
    for (const auto &layer : scene.terrain_layers) {
      auto item = prepare(base, layer.material, true, true,
                          layer.mask_texture, layer.world_to_uv,
                          {.55f, .55f, .55f}, false);
      group->base.push_back(std::move(item));
    }
  }
  for (const auto &draw : scene.draws) {
    if (!draw.index_count ||
        scene.library.materials.at(draw.material).blend == territory::Blend::Invisible)
      continue;
    const auto &material = group->materials.at(draw.material);
    auto effective = draw;
    bool geometric_normals = false;
    const auto linear = glm::mat3(draw.transform);
    const auto determinant = glm::determinant(linear);
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12f) {
      if (linear == glm::mat3{0.f}) {
        ++group->collapsed_skipped;
        continue;
      }
      auto rebuilt = territory::rebuild_geometric_normals(
          scene.meshes[draw.mesh], draw);
      if (rebuilt.mesh.indices.empty()) {
        ++group->collapsed_skipped;
        continue;
      }
      effective.mesh = group->meshes.size();
      effective.first_index = 0;
      effective.index_count = rebuilt.mesh.indices.size();
      group->meshes.push_back(std::make_unique<GpuMesh>(rebuilt.mesh));
      geometry::Box bounds;
      for (const auto &vertex : rebuilt.mesh.vertices)
        bounds += vertex.position;
      mesh_bounds.push_back(bounds);
      geometric_normals = true;
      ++group->singular_recovered;
    }
    auto item = prepare(effective, draw.material, false, false, 0, glm::mat4{1},
                        draw.water ? glm::vec3{.28f, .38f, .47f}
                                   : glm::vec3{.62f}, geometric_normals);
    (transparent(material.blend) ? group->alpha : group->opaque)
        .push_back(std::move(item));
  }
  std::stable_sort(group->opaque.begin(), group->opaque.end(),
                   [](const Item &a, const Item &b) {
                     if (a.program != b.program) return a.program < b.program;
                     return a.mesh < b.mesh;
                   });
  m_impl->groups.insert_or_assign(group_id, std::move(group));
}

void LiveVisualRenderer::remove(rendering::SceneGroupId group) {
  m_impl->groups.erase(group);
  m_impl->prune_textures();
  if (m_impl->groups.empty()) m_impl->shadow.reset();
}

void LiveVisualRenderer::render(const rendering::Camera &camera,
                                const LiveSceneSettings &settings,
                                LiveSceneDiagnostics &diagnostics) {
  LegacyBindingScope binding_scope{m_impl->context};
  diagnostics = {};
  if (!settings.shadows) m_impl->shadow.reset();
  const auto view_projection = camera.projection_matrix() * camera.view_matrix();
  const auto frustum = camera.frustum();
  bool use_shadows = settings.shadows && !m_impl->groups.empty();
  glm::mat4 light_matrix{1};
  glm::vec3 sun_direction{};
  if (use_shadows) {
    try {
      const auto prepare_shadow_color = [&](Group &group, const Item &item) {
        if (item.shadow_program) return;
        const auto key = std::pair{item.material, item.layer};
        auto cached = group.shadow_programs_by_material.find(key);
        if (cached == group.shadow_programs_by_material.end()) {
          const auto program_id = m_impl->get_program(
              group, group.materials.at(item.material), item.layer, true);
          cached = group.shadow_programs_by_material.emplace(key, program_id).first;
        }
        item.shadow_program = cached->second;
      };
      for (const auto &[group_id, group] : m_impl->groups) {
        (void)group_id;
        for (const auto &item : group->base) prepare_shadow_color(*group, item);
        for (const auto &item : group->opaque) prepare_shadow_color(*group, item);
        for (const auto &item : group->alpha) prepare_shadow_color(*group, item);
      }
      if (!m_impl->shadow)
        m_impl->shadow = std::make_unique<ShadowResources>(
            2048, m_impl->max_shadow_texture_size_for_test);
      auto coverage = m_impl->groups.begin()->second->bounds;
      for (const auto &[group_id, group] : m_impl->groups) {
        (void)group_id;
        const auto &bounds = group->bounds;
        coverage.min_x = std::min(coverage.min_x, bounds.min_x);
        coverage.min_y = std::min(coverage.min_y, bounds.min_y);
        coverage.min_z = std::min(coverage.min_z, bounds.min_z);
        coverage.max_x = std::max(coverage.max_x, bounds.max_x);
        coverage.max_y = std::max(coverage.max_y, bounds.max_y);
        coverage.max_z = std::max(coverage.max_z, bounds.max_z);
      }
      const auto projection = territory::make_shadow_projection(
          coverage, settings.sun_azimuth_deg, settings.sun_elevation_deg,
          m_impl->shadow->size);
      light_matrix = projection.relative_world_to_clip *
                     glm::translate(glm::mat4{1},
                                    {-static_cast<float>(coverage.min_x),
                                     -static_cast<float>(coverage.min_y), 0.f});
      sun_direction = projection.surface_to_sun;
      diagnostics.shadow_map_size = m_impl->shadow->size;
      glBindFramebuffer(GL_FRAMEBUFFER, m_impl->shadow->framebuffer);
      glViewport(0, 0, m_impl->shadow->size, m_impl->shadow->size);
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
      glDepthMask(GL_TRUE);
      glDisable(GL_BLEND);
      if (settings.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1.25f, 2.f);
      glClearDepth(1.0);
      glClear(GL_DEPTH_BUFFER_BIT);
      const auto cast = [&](Group &group, const Item &item) {
        if (item.water || !visible(settings, item)) return;
        const auto &material = group.materials.at(item.material);
        if (!material.shadow_coverage_reliable) {
          ++diagnostics.omitted_casters;
          return;
        }
        const auto masked = material.alpha_test ||
                            material.blend == territory::Blend::Masked;
        const auto depth_key = masked ? item.material
                                      : std::numeric_limits<std::size_t>::max();
        auto cached = group.depth_programs_by_material.find(depth_key);
        if (cached == group.depth_programs_by_material.end()) {
          const auto program_id = m_impl->get_depth_program(group, material,
                                                              masked);
          cached = group.depth_programs_by_material
                       .emplace(depth_key, program_id).first;
        }
        const auto id = cached->second;
        const auto &u = group.uniforms.at(id);
        glUseProgram(id);
        matrix(u.model, item.model);
        matrix(u.view_projection, light_matrix);
        matrix(u.world_to_uv, item.world_to_uv);
        glUniform2f(u.region_origin,
                    static_cast<float>(group.bounds.min_x),
                    static_cast<float>(group.bounds.min_y));
        glUniform2f(u.region_size,
                    static_cast<float>(group.bounds.max_x-group.bounds.min_x),
                    static_cast<float>(group.bounds.max_y-group.bounds.min_y));
        glUniform1i(u.terrain, item.terrain);
        if (masked) {
          glUniform1f(u.alpha_ref, material.alpha_ref);
          int unit{};
          for (const auto &[texture_id, sampler] : u.samplers) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, group.textures.at(texture_id)->id);
            glUniform1i(sampler, unit++);
          }
          for (const auto &[node, location] : u.uv_matrices)
            glUniformMatrix3fv(location, 1, GL_FALSE,
                               glm::value_ptr(material.nodes[node].uv));
        }
        glFrontFace(item.mirrored ? GL_CW : GL_CCW);
        if (material.two_sided || item.terrain) glDisable(GL_CULL_FACE);
        else { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
        glBindVertexArray(group.meshes.at(item.mesh)->vao);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.count),
                       GL_UNSIGNED_INT,
                       reinterpret_cast<const void *>(item.first*sizeof(std::uint32_t)));
        ++diagnostics.draws;
      };
      for (const auto &[group_id, group] : m_impl->groups) {
        (void)group_id;
        if (!group->base.empty()) cast(*group, group->base.front());
        for (const auto &item : group->opaque) cast(*group, item);
      }
      glDisable(GL_POLYGON_OFFSET_FILL);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, m_impl->context.framebuffer.size.width,
                 m_impl->context.framebuffer.size.height);
      if (settings.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } catch (const std::exception &error) {
      glDisable(GL_POLYGON_OFFSET_FILL);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, m_impl->context.framebuffer.size.width,
                 m_impl->context.framebuffer.size.height);
      if (settings.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      diagnostics.error = error.what();
      diagnostics.shadow_map_size = 0;
      use_shadows = false;
    }
  }
  glEnable(GL_DEPTH_TEST);
  GLint max_units{};
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_units);
  GLuint last_program{}, last_vao{};
  const Group *last_group{};
  std::size_t last_material = std::numeric_limits<std::size_t>::max();
  bool last_layer{}, last_terrain{};
  std::size_t last_mask{};
  const auto draw_item = [&](Group &group, const Item &item) {
    if (!visible(settings, item)) return;
    if (item.water && !settings.water) return;
    if (settings.culling && !item.bounds.is_zero() &&
        !frustum.intersects(item.bounds)) return;
    const auto &material = group.materials.at(item.material);
    const auto id = use_shadows ? item.shadow_program : item.program;
    const auto &u = group.uniforms.at(id);
    const auto program_changed = id != last_program;
    const auto material_changed = program_changed || last_group != &group ||
                                  last_material != item.material ||
                                  last_layer != item.layer ||
                                  last_terrain != item.terrain ||
                                  last_mask != item.mask_texture;
    if (program_changed) {
      glUseProgram(id);
      matrix(u.view_projection, view_projection);
      glUniform1i(u.show_textures, settings.textures);
      if (use_shadows) {
        matrix(u.light_matrix, light_matrix);
        glUniform3fv(u.sun_direction, 1, glm::value_ptr(sun_direction));
        glUniform1f(u.shadow_texel, 1.f / m_impl->shadow->size);
        glActiveTexture(GL_TEXTURE0 + max_units - 1);
        glBindTexture(GL_TEXTURE_2D, m_impl->shadow->depth);
        glUniform1i(u.shadow_map, max_units - 1);
      }
      last_program = id;
    }
    matrix(u.model, item.model);
    glUniformMatrix3fv(u.normal_matrix, 1, GL_FALSE,
                       glm::value_ptr(item.normal_matrix));
    matrix(u.world_to_uv, item.world_to_uv);
    glUniform3fv(u.flat_color, 1, glm::value_ptr(item.flat));
    if (material_changed) {
      glUniform2f(u.region_origin, static_cast<float>(group.bounds.min_x),
                  static_cast<float>(group.bounds.min_y));
      glUniform2f(u.region_size,
                  static_cast<float>(group.bounds.max_x - group.bounds.min_x),
                  static_cast<float>(group.bounds.max_y - group.bounds.min_y));
      glUniform1i(u.terrain, item.terrain);
      glUniform1i(u.alpha_test, material.alpha_test ||
                                     material.blend == territory::Blend::Masked);
      glUniform1f(u.alpha_ref, material.alpha_ref);
      glUniform1i(u.unlit, material.unlit);
      int unit{};
      for (const auto &[texture_id, sampler] : u.samplers) {
        if (unit >= max_units - int(use_shadows))
          throw std::runtime_error{"Live material exceeds GPU samplers"};
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, group.textures.at(texture_id)->id);
        glUniform1i(sampler, unit++);
      }
      for (const auto &[node, location] : u.uv_matrices)
        glUniformMatrix3fv(location, 1, GL_FALSE,
                           glm::value_ptr(material.nodes[node].uv));
      if (item.layer) {
        if (unit >= max_units - int(use_shadows))
          throw std::runtime_error{"Live terrain mask exceeds GPU samplers"};
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, group.textures.at(item.mask_texture)->id);
        glUniform1i(u.terrain_mask, unit);
      }
      if (material.depth_test || item.terrain) glEnable(GL_DEPTH_TEST);
      else glDisable(GL_DEPTH_TEST);
      glDepthFunc(item.layer ? GL_LEQUAL : GL_LESS);
      glDepthMask(item.layer ? GL_FALSE : GLboolean(material.depth_write));
      last_group = &group;
      last_material = item.material;
      last_layer = item.layer;
      last_terrain = item.terrain;
      last_mask = item.mask_texture;
    }
    glFrontFace(item.mirrored ? GL_CW : GL_CCW);
    if (material.two_sided || item.terrain) glDisable(GL_CULL_FACE);
    else {
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
    }
    if (item.layer || transparent(material.blend)) {
      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      if (item.layer || material.blend == territory::Blend::Alpha)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      else if (material.blend == territory::Blend::Modulate)
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
      else if (material.blend == territory::Blend::Translucent)
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
      else
        glBlendFunc(GL_ONE, GL_ONE);
    } else glDisable(GL_BLEND);
    const auto vao = group.meshes.at(item.mesh)->vao;
    if (vao != last_vao) {
      glBindVertexArray(vao);
      last_vao = vao;
    }
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(item.count),
                   GL_UNSIGNED_INT,
                   reinterpret_cast<const void *>(item.first * sizeof(std::uint32_t)));
    ++diagnostics.draws;
  };
  for (const auto &[group_id, group] : m_impl->groups) {
    (void)group_id;
    for (const auto &item : group->base) draw_item(*group, item);
    for (const auto &item : group->opaque) draw_item(*group, item);
  }
  struct TransparentItem { Group *group; const Item *item; float distance; };
  std::vector<TransparentItem> transparent_items;
  const auto eye = camera.position();
  for (const auto &[group_id, group] : m_impl->groups) {
    (void)group_id;
    for (const auto &item : group->alpha) {
      const auto center = (item.bounds.min() + item.bounds.max()) * .5f;
      const auto delta = center - eye;
      transparent_items.push_back({group.get(), &item, glm::dot(delta, delta)});
    }
  }
  std::stable_sort(transparent_items.begin(), transparent_items.end(),
                   [](const auto &a, const auto &b) {
                     return a.distance > b.distance;
                   });
  for (const auto &entry : transparent_items)
    draw_item(*entry.group, *entry.item);
  m_impl->prune_textures();
  diagnostics.gpu_textures = static_cast<int>(m_impl->texture_cache.size());
  for (const auto &[group_id, group] : m_impl->groups) {
    (void)group_id;
    diagnostics.gpu_programs += static_cast<int>(group->programs.size());
    diagnostics.fallback_textures += group->fallback_textures;
    diagnostics.fallback_materials += group->fallback_materials;
    diagnostics.singular_recovered += group->singular_recovered;
    diagnostics.collapsed_skipped += group->collapsed_skipped;
  }
}

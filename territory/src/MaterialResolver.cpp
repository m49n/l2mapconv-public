#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <territory/MaterialResolver.h>
#include <unordered_map>
#include <unordered_set>
namespace territory {
namespace {
std::string identity(const unreal::AssetReference &r) {
  return r.package.empty() ? r.object_path : r.package + '.' + r.object_path;
}
struct Unsupported : std::runtime_error {
  using std::runtime_error::runtime_error;
};
} // namespace
struct MaterialResolver::Impl {
  MaterialLibrary &library;
  Report &report;
  std::function<bool(std::string_view)> package_probe;
  struct Cached {
    std::uint32_t id;
    std::vector<std::size_t> issues;
  };
  std::unordered_map<std::string, Cached> cache;
  std::unordered_map<std::string, std::uint32_t> texture_cache;
  std::unordered_set<const unreal::Object *> active;
  std::vector<std::string> reference_stack;
  std::vector<std::size_t> current_issues;
  std::string surface;
  Impl(MaterialLibrary &l, Report &r) : library(l), report(r) {}
  void issue(IssueKind kind, const std::string &source,
             const std::string &reason) {
    auto it = std::find_if(
        report.issues.begin(), report.issues.end(), [&](const Issue &i) {
          return i.kind == kind && i.source == source && i.reason == reason;
        });
    if (it == report.issues.end()) {
      report.issues.push_back({kind, source, reason, {}});
      it = std::prev(report.issues.end());
    }
    if (std::find(it->surfaces.begin(), it->surfaces.end(), surface) ==
        it->surfaces.end())
      it->surfaces.push_back(surface);
    current_issues.push_back(
        static_cast<std::size_t>(it - report.issues.begin()));
  }
  std::uint32_t node(RenderMaterial &m, Node n) {
    if (m.nodes.size() >= 256)
      throw Unsupported("material exceeds 256 nodes");
    m.nodes.push_back(std::move(n));
    return static_cast<std::uint32_t>(m.nodes.size() - 1);
  }
  std::uint32_t constant(RenderMaterial &m,
                         glm::vec4 color = glm::vec4(.5f, .5f, .5f, 1.f)) {
    Node n;
    n.value = color;
    return node(m, std::move(n));
  }
  std::uint32_t binary(RenderMaterial &m, Op op, std::uint32_t a,
                       std::uint32_t b) {
    Node n;
    n.op = op;
    n.inputs = {a, b};
    return node(m, std::move(n));
  }
  std::uint32_t child(RenderMaterial &m, const unreal::MaterialReference &ref,
                      TextureUsage usage, glm::mat3 uv, int channel,
                      unsigned depth) {
    if (!ref.has_reference())
      return constant(m, glm::vec4(1.f));
    const auto reference = ref.reference();
    try {
      return visit(m, ref.untyped(), reference, usage, uv, channel, depth);
    } catch (const Unsupported &e) {
      issue(IssueKind::Unsupported, identity(reference), e.what());
      return constant(m);
    } catch (const std::exception &e) {
      issue(IssueKind::Corrupt, identity(reference), e.what());
      return constant(m);
    }
  }
  std::uint32_t visit(RenderMaterial &m,
                      const std::shared_ptr<unreal::Object> &object,
                      const unreal::AssetReference &reference,
                      TextureUsage usage, glm::mat3 uv, int channel,
                      unsigned depth) {
    const auto source = identity(reference);
    m.reference_chain.push_back(
        {source, reference.class_name,
         reference_stack.empty() ? std::string{} : reference_stack.back(),
         object ? "resolved" : "missing", usage});
    if (!object) {
      if (!reference.object_path.empty()) {
        const bool absent = package_probe && !package_probe(reference.package);
        issue(absent ? IssueKind::MissingPackage : IssueKind::MissingObject,
              source,
              absent ? "referenced package not found"
                     : "referenced object not found");
      }
      return constant(m);
    }
    if (depth >= 64 || !active.insert(object.get()).second)
      throw Unsupported("material reference cycle/depth limit at " + source);
    struct ActiveScope {
      std::unordered_set<const unreal::Object *> &set;
      const unreal::Object *object;
      ~ActiveScope() { set.erase(object); }
    } scope{active, object.get()};
    reference_stack.push_back(source);
    struct ReferenceScope {
      std::vector<std::string> &stack;
      ~ReferenceScope() { stack.pop_back(); }
    } reference_scope{reference_stack};
    auto descend = [&](const unreal::MaterialReference &ref, TextureUsage u) {
      return child(m, ref, u, uv, channel, depth + 1);
    };
    if (auto solid = std::dynamic_pointer_cast<unreal::ConstantColor>(object)) {
      const auto &c = solid->color;
      return constant(m,
                      {srgb_to_linear(c.r / 255.f), srgb_to_linear(c.g / 255.f),
                       srgb_to_linear(c.b / 255.f), c.a / 255.f});
    }
    if (reference.class_name == "VertexColor") {
      Node n;
      n.op = Op::VertexColor;
      return node(m, std::move(n));
    }
    if (auto texture = std::dynamic_pointer_cast<unreal::Texture>(object)) {
      const auto key = source + '#' + std::to_string(static_cast<int>(usage));
      auto found = texture_cache.find(key);
      std::uint32_t index;
      if (found == texture_cache.end()) {
        try {
          auto data = texture_data(*texture, usage);
          index = static_cast<std::uint32_t>(library.textures.size());
          library.textures.push_back(std::move(data));
          texture_cache.emplace(key, index);
        } catch (const std::exception &e) {
          const auto f = texture->format;
          bool supported = f == unreal::TEXF_P8 || f == unreal::TEXF_RGBA8 ||
                           f == unreal::TEXF_DXT1 || f == unreal::TEXF_DXT3 ||
                           f == unreal::TEXF_DXT5 || f == unreal::TEXF_L8;
          issue(supported && !texture->mips.empty() ? IssueKind::Corrupt
                                                    : IssueKind::Unsupported,
                source, e.what());
          return constant(m);
        }
      } else
        index = found->second;
      if (texture->masked) {
        m.blend = Blend::Masked;
        m.alpha_test = true;
      } else if (texture->alpha_texture)
        m.blend = Blend::Alpha;
      m.two_sided = m.two_sided || texture->two_sided;
      Node n;
      n.op = Op::Sample;
      n.texture = index;
      n.uv = uv;
      n.uv_channel = channel;
      return node(m, std::move(n));
    }
    if (auto shader = std::dynamic_pointer_cast<unreal::Shader>(object)) {
      auto color = descend(shader->diffuse, usage);
      if (shader->opacity.has_reference())
        color = binary(m, Op::AlphaReplace, color,
                       descend(shader->opacity, TextureUsage::Mask));
      switch (shader->output_blending) {
      case unreal::OB_Normal:
        m.blend =
            shader->opacity.has_reference() ? Blend::Alpha : Blend::Opaque;
        break;
      case unreal::OB_Masked:
        m.blend = Blend::Masked;
        break;
      case unreal::OB_Modulate:
        m.blend = Blend::Modulate;
        break;
      case unreal::OB_Translucent:
        m.blend = Blend::Translucent;
        break;
      case unreal::OB_Invisible:
        m.blend = Blend::Invisible;
        break;
      case unreal::OB_Brighten:
        m.blend = Blend::Add;
        break;
      default:
        throw Unsupported("unsupported Shader OutputBlending " +
                          std::to_string(shader->output_blending));
      }
      m.alpha_test =
          shader->alpha_test || shader->output_blending == unreal::OB_Masked;
      if (m.alpha_test && m.blend == Blend::Opaque)
        m.blend = Blend::Masked;
      m.alpha_ref = shader->alpha_ref / 255.f;
      m.depth_write = shader->z_write;
      m.two_sided = shader->two_sided || shader->treat_as_two_sided;
      const unreal::MaterialReference *omitted[] = {
          &shader->specular, &shader->specular_mask, &shader->self_illumination,
          &shader->self_illumination_mask, &shader->detail};
      for (auto ref : omitted)
        if (ref->has_reference()) {
          const auto r = ref->reference();
          m.reference_chain.push_back({identity(r), r.class_name, source,
                                       "not_evaluated", TextureUsage::Data});
          issue(IssueKind::Simplified, identity(ref->reference()),
                "referenced by " + source +
                    ": effect not evaluated/inspected by neutral-light export");
        }
      if (shader->wire_frame)
        issue(IssueKind::Simplified, source, "wireframe effect not reproduced");
      return color;
    }
    if (auto blend = std::dynamic_pointer_cast<unreal::FinalBlend>(object)) {
      auto color = descend(blend->material, usage);
      switch (blend->fb_blending) {
      case unreal::FB_Overwrite:
        m.blend = Blend::Opaque;
        break;
      case unreal::FB_Modulate:
        m.blend = Blend::Modulate;
        break;
      case unreal::FB_AlphaBlend:
        m.blend = Blend::Alpha;
        break;
      case unreal::FB_Translucent:
        m.blend = Blend::Translucent;
        break;
      case unreal::FB_Brighten:
      case 8:
        m.blend = Blend::Add;
        break;
      case unreal::FB_Invisible:
        m.blend = Blend::Invisible;
        break;
      default:
        throw Unsupported("unsupported FinalBlend mode " +
                          std::to_string(blend->fb_blending));
      }
      m.alpha_test = blend->alpha_test;
      if (m.alpha_test && m.blend == Blend::Opaque)
        m.blend = Blend::Masked;
      m.alpha_ref = blend->alpha_ref / 255.f;
      m.depth_write = blend->z_write;
      m.depth_test = blend->z_test;
      m.two_sided = blend->two_sided || blend->treat_as_two_sided;
      return color;
    }
    if (auto modifier = std::dynamic_pointer_cast<unreal::Modifier>(object)) {
      glm::mat3 local(1.f);
      if (reference.class_name == "TexScaler") {
        if (!std::isfinite(modifier->u_scale) ||
            !std::isfinite(modifier->v_scale) || modifier->u_scale == 0 ||
            modifier->v_scale == 0)
          throw Unsupported("invalid texture scale");
        local[0][0] = 1.f / modifier->u_scale;
        local[1][1] = 1.f / modifier->v_scale;
        local[2][0] = -modifier->u_offset / modifier->u_scale;
        local[2][1] = -modifier->v_offset / modifier->v_scale;
      } else if (reference.class_name == "TexRotator") {
        if (modifier->rotation_type != 0 || modifier->rotation.pitch != 0 ||
            modifier->rotation.roll != 0)
          throw Unsupported("unsupported animated/3D texture rotation");
        const float angle = static_cast<float>(modifier->rotation.yaw) *
                            6.28318530718f / 65536.f;
        const float c = std::cos(angle), s = std::sin(angle);
        local[0] = {c, s, 0};
        local[1] = {-s, c, 0};
        local[2] = {modifier->u_offset * (1 - c) + modifier->v_offset * s,
                    modifier->v_offset * (1 - c) - modifier->u_offset * s, 1};
      } else if (reference.class_name == "TexPanner" ||
                 reference.class_name == "TexPannerTriggered") {
        issue(IssueKind::Simplified, source,
              "texture panning sampled at fixed time 0 seconds");
      } else if (reference.class_name == "TexOscillator" ||
                 reference.class_name == "TexOscillatorTriggered") {
        issue(IssueKind::Simplified, source,
              "UV oscillation disabled; using unanimated base coordinates, not "
              "retail animation parity");
      } else if (reference.class_name != "TexModifier" &&
                 reference.class_name != "TexCoordSource") {
        throw Unsupported("unsupported UV modifier " + reference.class_name);
      }
      if (modifier->tex_coord_source > 7)
        throw Unsupported("unsupported generated texture coordinates");
      if (modifier->tex_coord_source != 0)
        channel = modifier->tex_coord_source;
      return child(m, modifier->material, usage, local * uv, channel,
                   depth + 1);
    }
    if (auto combiner = std::dynamic_pointer_cast<unreal::Combiner>(object)) {
      if (combiner->combine_operation > 7 || combiner->alpha_operation > 4)
        throw Unsupported("unsupported Combiner enum");
      auto a = descend(combiner->material1, usage),
           b = descend(combiner->material2, usage),
           mask = descend(combiner->mask, TextureUsage::Mask);
      if (combiner->invert_mask)
        mask = binary(m, Op::Subtract, constant(m, glm::vec4(1.f)), mask);
      auto color = a;
      switch (combiner->combine_operation) {
      case 0:
        break;
      case 1:
        color = b;
        break;
      case 2:
        color = binary(m, Op::Multiply, a, b);
        break;
      case 3:
        color = binary(m, Op::Add, a, b);
        break;
      case 4:
        color = binary(m, Op::Subtract, a, b);
        break;
      case 5: {
        Node n;
        n.op = Op::Lerp;
        n.inputs = {a, b, mask};
        color = node(m, std::move(n));
        break;
      }
      case 6:
        color = binary(m, Op::Add, a, binary(m, Op::Multiply, b, mask));
        break;
      case 7:
        color = mask;
        break;
      }
      if (combiner->modulate_2x || combiner->modulate_4x)
        color =
            binary(m, Op::Multiply, color,
                   constant(m, glm::vec4(combiner->modulate_4x ? 4.f : 2.f)));
      auto alpha = mask;
      switch (combiner->alpha_operation) {
      case 0:
        break;
      case 1:
        alpha = binary(m, Op::Multiply, a, b);
        break;
      case 2:
        alpha = binary(m, Op::Add, a, b);
        break;
      case 3:
        alpha = a;
        break;
      case 4:
        alpha = b;
        break;
      }
      return binary(m, Op::AlphaReplace, color, alpha);
    }
    auto material = std::dynamic_pointer_cast<unreal::Material>(object);
    if (material && material->fallback_material.has_reference()) {
      issue(IssueKind::Simplified, source,
            "using explicit client FallbackMaterial");
      return descend(material->fallback_material, usage);
    }
    throw Unsupported("unsupported material class " + reference.class_name);
  }
  std::uint32_t resolve(const std::shared_ptr<unreal::Object> &object,
                        const unreal::AssetReference &ref,
                        std::string_view label) {
    surface = label;
    current_issues.clear();
    const auto key = identity(ref) + '#' + ref.class_name;
    if (auto found = cache.find(key); found != cache.end()) {
      for (auto index : found->second.issues) {
        auto &targets = report.issues.at(index).surfaces;
        if (std::find(targets.begin(), targets.end(), surface) == targets.end())
          targets.push_back(surface);
      }
      return found->second.id;
    }
    RenderMaterial material;
    material.source = identity(ref);
    material.class_name = ref.class_name;
    active.clear();
    reference_stack.clear();
    try {
      material.root = visit(material, object, ref, TextureUsage::Color,
                            glm::mat3(1.f), 0, 0);
      (void)material_expression(material);
      std::set<std::uint32_t> samples;
      for (const auto &n : material.nodes)
        if (n.texture) {
          if (*n.texture >= library.textures.size())
            throw Unsupported("material texture index outside library");
          samples.insert(*n.texture);
        }
      if (samples.size() > 16)
        throw Unsupported(
            "material exceeds baseline 16 fragment texture samplers");
    } catch (const std::exception &e) {
      issue(IssueKind::Unsupported, material.source, e.what());
      auto chain = std::move(material.reference_chain);
      material = RenderMaterial{};
      material.source = identity(ref);
      material.class_name = ref.class_name;
      material.reference_chain = std::move(chain);
      material.root = constant(material);
    }
    const auto id = static_cast<std::uint32_t>(library.materials.size());
    library.materials.push_back(std::move(material));
    cache.emplace(key, Cached{id, current_issues});
    return id;
  }
};
MaterialResolver::MaterialResolver(MaterialLibrary &l, Report &r)
    : m_impl(std::make_unique<Impl>(l, r)) {}
MaterialResolver::~MaterialResolver() = default;
std::uint32_t
MaterialResolver::resolve(const std::shared_ptr<unreal::Material> &p,
                          const unreal::AssetReference &r, std::string_view s) {
  return m_impl->resolve(p, r, s);
}
std::uint32_t
MaterialResolver::resolve_object(const std::shared_ptr<unreal::Object> &p,
                                 const unreal::AssetReference &r,
                                 std::string_view s) {
  return m_impl->resolve(p, r, s);
}
void MaterialResolver::set_package_probe(
    std::function<bool(std::string_view)> probe) {
  m_impl->package_probe = std::move(probe);
}
} // namespace territory

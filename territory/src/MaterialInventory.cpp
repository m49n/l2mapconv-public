#include <map>
#include <set>
#include <territory/VisualScene.h>
namespace territory {
namespace {
const char *usage_name(TextureUsage u) {
  switch (u) {
  case TextureUsage::Color:
    return "color_srgb";
  case TextureUsage::Mask:
    return "mask_linear";
  case TextureUsage::Data:
    return "data_linear";
  }
  return "unknown";
}
const char *encoding_name(PixelEncoding e) {
  switch (e) {
  case PixelEncoding::Rgba8:
    return "rgba8";
  case PixelEncoding::R8:
    return "r8";
  case PixelEncoding::Dxt1:
    return "dxt1";
  case PixelEncoding::Dxt3:
    return "dxt3";
  case PixelEncoding::Dxt5:
    return "dxt5";
  }
  return "unknown";
}
const char *op_name(Op op) {
  switch (op) {
  case Op::Constant:
    return "constant";
  case Op::Sample:
    return "sample";
  case Op::Multiply:
    return "multiply";
  case Op::Add:
    return "add";
  case Op::Subtract:
    return "subtract";
  case Op::Lerp:
    return "lerp";
  case Op::AlphaReplace:
    return "alpha_replace";
  case Op::VertexColor:
    return "vertex_color";
  }
  return "unknown";
}
const char *blend_name(Blend b) {
  switch (b) {
  case Blend::Opaque:
    return "opaque";
  case Blend::Masked:
    return "masked";
  case Blend::Alpha:
    return "alpha";
  case Blend::Modulate:
    return "modulate";
  case Blend::Translucent:
    return "translucent";
  case Blend::Add:
    return "add";
  case Blend::Invisible:
    return "invisible";
  }
  return "unknown";
}
int severity(IssueKind k) {
  switch (k) {
  case IssueKind::Simplified:
    return 1;
  case IssueKind::Unsupported:
  case IssueKind::WaterUnresolved:
    return 2;
  case IssueKind::MissingPackage:
  case IssueKind::MissingObject:
    return 3;
  case IssueKind::Corrupt:
    return 4;
  }
  return 0;
}
const char *state_name(int level) {
  const char *states[] = {"supported", "simplified", "unsupported", "missing",
                          "corrupt"};
  return states[level];
}
} // namespace
Json material_inventory(const VisualScene &scene) {
  Json result = {{"materials", Json::array()}, {"textures", Json::array()}};
  std::map<std::size_t, std::set<std::string>> uses;
  for (const auto &draw : scene.draws)
    if (draw.index_count)
      uses[draw.material].insert(draw.source);
  if (scene.terrain_mesh)
    for (std::size_t i = 0; i < scene.terrain_layers.size(); ++i)
      uses[scene.terrain_layers[i].material].insert("terrain:layer:" +
                                                    std::to_string(i));
  std::map<std::string, int> source_states, surface_states;
  for (const auto &issue : scene.report.issues) {
    source_states[issue.source] =
        std::max(source_states[issue.source], severity(issue.kind));
    for (const auto &surface : issue.surfaces)
      surface_states[surface] =
          std::max(surface_states[surface], severity(issue.kind));
  }
  std::map<std::string, std::size_t> deduplicated;
  std::vector<std::set<std::string>> surfaces;
  std::map<std::size_t, std::set<std::size_t>> texture_uses;
  for (const auto &[id, targets] : uses) {
    const auto &m = scene.library.materials.at(id);
    Json graph = Json::array(), chain = Json::array();
    bool textured = false;
    int state = source_states[m.source];
    for (const auto &target : targets)
      state = std::max(state, surface_states[target]);
    for (const auto &step : m.reference_chain) {
      state = std::max(state, source_states[step.source]);
      chain.push_back({{"source", step.source},
                       {"class", step.class_name},
                       {"parent", step.parent},
                       {"resolution", step.state},
                       {"state", source_states[step.source]
                                     ? state_name(source_states[step.source])
                                     : step.state},
                       {"usage", usage_name(step.usage)}});
    }
    for (const auto &node : m.nodes) {
      Json uv = Json::array();
      for (int c = 0; c < 3; ++c)
        uv.push_back({node.uv[c][0], node.uv[c][1], node.uv[c][2]});
      graph.push_back(
          {{"op", op_name(node.op)},
           {"inputs", node.inputs},
           {"value", {node.value.x, node.value.y, node.value.z, node.value.w}},
           {"texture", node.texture ? Json(*node.texture) : Json(nullptr)},
           {"uv_channel", node.uv_channel},
           {"uv_columns", uv}});
      textured = textured || node.op == Op::Sample;
    }
    Json item = {{"source", m.source},
                 {"class", m.class_name.empty() ? "generated" : m.class_name},
                 {"state", state ? state_name(state)
                                 : (textured ? "supported" : "non_textured")},
                 {"reference_chain", chain},
                 {"nodes", graph},
                 {"root", m.root},
                 {"blend", blend_name(m.blend)},
                 {"alpha_test", m.alpha_test},
                 {"alpha_ref", m.alpha_ref},
                 {"depth_write", m.depth_write},
                 {"depth_test", m.depth_test},
                 {"two_sided", m.two_sided},
                 {"unlit", m.unlit}};
    const auto key = item.dump();
    auto [entry, inserted] =
        deduplicated.emplace(key, result["materials"].size());
    const auto index = entry->second;
    if (inserted) {
      item["id"] = index;
      item["library_ids"] = Json::array();
      result["materials"].push_back(std::move(item));
      surfaces.emplace_back();
    }
    result["materials"][index]["library_ids"].push_back(id);
    surfaces[index].insert(targets.begin(), targets.end());
    for (const auto &node : m.nodes)
      if (node.texture)
        texture_uses[*node.texture].insert(index);
  }
  for (std::size_t i = 0; i < surfaces.size(); ++i)
    result["materials"][i]["surfaces"] = surfaces[i];
  std::map<std::size_t, std::vector<std::size_t>> mask_uses;
  if (scene.terrain_mesh)
    for (std::size_t i = 0; i < scene.terrain_layers.size(); ++i)
      mask_uses[scene.terrain_layers[i].mask_texture].push_back(i);
  for (std::size_t i = 0; i < scene.library.textures.size(); ++i) {
    const auto &t = scene.library.textures[i];
    result["textures"].push_back({{"id", i},
                                  {"source", t.source},
                                  {"width", t.width},
                                  {"height", t.height},
                                  {"encoding", encoding_name(t.encoding)},
                                  {"usage", usage_name(t.usage)},
                                  {"payload_bytes", t.bytes.size()},
                                  {"clamp_u", t.clamp_u},
                                  {"clamp_v", t.clamp_v},
                                  {"material_ids", texture_uses[i]},
                                  {"terrain_mask_layers", mask_uses[i]}});
  }
  return result;
}
} // namespace territory

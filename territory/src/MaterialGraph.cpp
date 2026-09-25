#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <territory/MaterialGraph.h>
namespace territory {
std::vector<int> neutralize_missing_uv_samples(
    RenderMaterial &material, const std::array<bool, 4> &available_channels) {
  std::vector<int> missing;
  for (auto &node : material.nodes)
    if (node.op == Op::Sample &&
        (node.uv_channel < 0 || node.uv_channel >= 4 ||
         !available_channels[node.uv_channel])) {
      missing.push_back(node.uv_channel);
      node = Node{};
      node.value = {.5f, .5f, .5f, 1.f};
    }
  if (!missing.empty())
    material.shadow_coverage_reliable = false;
  return missing;
}
glm::vec4 evaluate_node(Op op, glm::vec4 a, glm::vec4 b, float m) {
  switch (op) {
  case Op::Multiply:
    return a * b;
  case Op::Add:
    return a + b;
  case Op::Subtract:
    return a - b;
  case Op::Lerp:
    return a * (1.f - m) + b * m;
  case Op::AlphaReplace:
    return {a.r, a.g, a.b, b.a};
  default:
    throw std::invalid_argument("operation is not binary");
  }
}
std::string material_expression(const RenderMaterial &material) {
  if (material.nodes.empty() || material.nodes.size() > 256 ||
      material.root >= material.nodes.size())
    throw std::invalid_argument("invalid material graph size/root");
  std::vector<unsigned char> visited(material.nodes.size());
  std::ostringstream text;
  text.imbue(std::locale::classic());
  text << std::setprecision(9) << std::showpoint;
  std::function<void(std::uint32_t, unsigned)> emit = [&](std::uint32_t i,
                                                          unsigned depth) {
    if (i >= material.nodes.size() || depth > 64)
      throw std::invalid_argument("material graph depth/index limit");
    if (visited[i] == 1)
      throw std::invalid_argument("material graph cycle");
    if (visited[i] == 2)
      return;
    visited[i] = 1;
    const auto &n = material.nodes[i];
    const auto arity =
        n.op == Op::Constant || n.op == Op::Sample || n.op == Op::VertexColor
            ? 0U
        : n.op == Op::Lerp ? 3U
                           : 2U;
    if (n.inputs.size() != arity || n.uv_channel < 0 || n.uv_channel > 7)
      throw std::invalid_argument("invalid material node inputs/UV channel");
    for (int j = 0; j < 4; ++j)
      if (!std::isfinite(n.value[j]))
        throw std::invalid_argument("non-finite material constant");
    for (int a = 0; a < 3; ++a)
      for (int b = 0; b < 3; ++b)
        if (!std::isfinite(n.uv[a][b]))
          throw std::invalid_argument("non-finite UV matrix");
    for (auto input : n.inputs)
      emit(input, depth + 1);
    auto input = [&](unsigned index) {
      return "node_" + std::to_string(n.inputs.at(index));
    };
    text << "vec4 node_" << i << " = ";
    switch (n.op) {
    case Op::VertexColor:
      text << "v_color";
      break;
    case Op::Constant:
      text << "vec4(" << n.value.r << ',' << n.value.g << ',' << n.value.b
           << ',' << n.value.a << ')';
      break;
    case Op::Sample:
      if (!n.texture || *n.texture > 1000000)
        throw std::invalid_argument("invalid material texture index");
      text << "texture(tex_" << *n.texture << ", (uv_" << i << " * vec3(v_uv["
           << n.uv_channel << "],1.0)).xy)";
      break;
    case Op::Multiply:
      text << input(0) << " * " << input(1);
      break;
    case Op::Add:
      text << input(0) << " + " << input(1);
      break;
    case Op::Subtract:
      text << input(0) << " - " << input(1);
      break;
    case Op::Lerp:
      text << "mix(" << input(0) << ',' << input(1) << ',' << input(2) << ".a)";
      break;
    case Op::AlphaReplace:
      text << "vec4(" << input(0) << ".rgb," << input(1) << ".a)";
      break;
    default:
      throw std::invalid_argument("unknown material operation");
    }
    text << ";\n";
    visited[i] = 2;
  };
  emit(material.root, 0);
  text << "return node_" << material.root << ";\n";
  return text.str();
}
} // namespace territory

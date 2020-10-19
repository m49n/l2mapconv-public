#include "pch.h"

#include <rendering/ShaderLoader.h>

namespace rendering {

ShaderLoader::ShaderLoader(Context &context,
                           const std::filesystem::path &root_path)
    : m_context{context}, m_root_path{root_path} {}

auto ShaderLoader::load_entity_shader(const std::string &name) const
    -> std::shared_ptr<EntityShader> {

  const auto pair = m_entity_shaders.find(name);

  if (pair != m_entity_shaders.end()) {
    return pair->second;
  }

  auto shader = std::make_shared<EntityShader>(load_shader(name));
  m_entity_shaders[name] = shader;
  return shader;
}

auto ShaderLoader::load_shader(const std::string &name) const -> Shader {
  const auto vertex =
      read_file(m_root_path / std::filesystem::path{name + ".vert"});
  const auto fragment =
      read_file(m_root_path / std::filesystem::path{name + ".frag"});
  const auto geometry =
      read_file(m_root_path / std::filesystem::path{name + ".geom"}, true);

  return Shader{m_context, vertex, fragment, geometry};
}

auto ShaderLoader::read_file(const std::filesystem::path &path,
                             bool optional) const -> std::string {

  std::ifstream input{path};

  if (!input.is_open()) {
    ASSERT(optional, "Rendering", "Shader file not found: " << path);
    return "";
  }

  std::stringstream content;
  content << input.rdbuf();
  return content.str();
}

} // namespace rendering

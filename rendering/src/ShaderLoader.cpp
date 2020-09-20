#include "pch.h"

#include <rendering/ShaderLoader.h>

namespace rendering {

ShaderLoader::ShaderLoader(Context &context,
                           const std::filesystem::path &root_path)
    : m_context{context}, m_root_path{root_path} {}

auto ShaderLoader::load_shader(const std::string &name) const
    -> std::shared_ptr<EntityShader> {

  const auto pair = m_shaders.find(name);

  if (pair != m_shaders.end()) {
    return pair->second;
  }

  const auto vertex =
      read_file(m_root_path / std::filesystem::path{name + ".vert"});
  const auto fragment =
      read_file(m_root_path / std::filesystem::path{name + ".frag"});

  auto shader = std::make_shared<EntityShader>(m_context, vertex, fragment);

  m_shaders[name] = shader;
  return shader;
}

auto ShaderLoader::read_file(const std::filesystem::path &path) const
    -> std::string {

  std::ifstream input{path};
  ASSERT(input.is_open(), "App", "Shader file not found: " << path);
  std::stringstream content;
  content << input.rdbuf();
  return content.str();
}

} // namespace rendering

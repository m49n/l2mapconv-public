#pragma once

#include "Context.h"
#include "EntityShader.h"

#include <utils/NonCopyable.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

namespace rendering {

class ShaderLoader : public utils::NonCopyable {
public:
  explicit ShaderLoader(Context &context,
                        const std::filesystem::path &root_path);

  auto load_shader(const std::string &name) const
      -> std::shared_ptr<EntityShader>;

private:
  Context &m_context;
  const std::filesystem::path m_root_path;

  mutable std::unordered_map<std::string, std::shared_ptr<EntityShader>>
      m_shaders;

  auto read_file(const std::filesystem::path &path) const -> std::string;
};

} // namespace rendering

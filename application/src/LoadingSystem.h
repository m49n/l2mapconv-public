#pragma once

#include "Entity.h"
#include "RenderingContext.h"
#include "System.h"

#include <rendering/Texture.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class LoadingSystem : public System {
public:
  explicit LoadingSystem(RenderingContext &rendering_context,
                         const std::filesystem::path &root_path,
                         const std::vector<std::string> &maps);

private:
  RenderingContext &m_rendering_context;

  auto load_texture(const Texture &texture)
      -> std::shared_ptr<rendering::Texture>;
};

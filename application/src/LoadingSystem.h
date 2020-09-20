#pragma once

#include "RenderingContext.h"
#include "System.h"

#include <filesystem>

class LoadingSystem : public System {
public:
  explicit LoadingSystem(RenderingContext &rendering_context,
                         const std::filesystem::path &root_path);

private:
  RenderingContext &m_rendering_context;
};

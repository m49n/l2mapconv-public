#pragma once

#include "RenderingContext.h"
#include "System.h"

#include <filesystem>
#include <string>
#include <vector>

class LoadingSystem : public System {
public:
  explicit LoadingSystem(RenderingContext &rendering_context,
                         const std::filesystem::path &root_path,
                         const std::vector<std::string> &maps);

private:
  RenderingContext &m_rendering_context;
};

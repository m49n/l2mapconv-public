#pragma once

#include <territory/MaterialGraph.h>

#include <string>

namespace live_shaders {
auto vertex() -> const std::string &;
auto color_fragment(const territory::RenderMaterial &material,
                    bool terrain_mask, bool shadows) -> std::string;
auto depth_fragment(const territory::RenderMaterial &material) -> std::string;
}

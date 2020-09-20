#pragma once

#include <cstdint>

namespace rendering {

struct FrameSettings {
  std::uint64_t surface_filter;
  bool wireframe;
};

} // namespace rendering

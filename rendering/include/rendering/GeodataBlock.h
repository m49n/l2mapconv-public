#pragma once

#include <cstdint>

namespace rendering {

struct GeodataBlock {
  std::int32_t x;
  std::int32_t y;
  std::int32_t z;
  std::uint16_t type;
  bool north : 1;
  bool south : 1;
  bool west : 1;
  bool east : 1;
};

} // namespace rendering

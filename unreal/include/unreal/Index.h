#pragma once

#include <cstdint>
#include <ostream>

namespace unreal {

struct Index {
  std::int32_t value;

  operator std::int32_t() const { return value; }

  friend auto operator<<(std::ostream &output, const Index &index)
      -> std::ostream &;
};

} // namespace unreal

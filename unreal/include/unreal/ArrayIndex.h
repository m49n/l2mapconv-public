#pragma once

#include <cstdint>

namespace unreal {

class Archive;

struct ArrayIndex {
  std::int32_t value;

  operator std::int32_t() const { return value; }

  friend auto operator>>(Archive &archive, ArrayIndex &array_index)
      -> Archive &;
};

} // namespace unreal

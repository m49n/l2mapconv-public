#pragma once

#include "ArrayIndex.h"
#include "Index.h"
#include "Name.h"
#include "Primitives.h"

#include <utils/Assert.h>

#include <cstdint>
#include <ostream>
#include <unordered_map>
#include <vector>

namespace unreal {

enum class PropertyType : std::uint8_t {
  Byte = 1,
  Int = 2,
  Bool = 3,
  Float = 4,
  Object = 5,
  Name = 6,
  Delegate = 7,
  Class = 8,
  Array = 9,
  Struct = 10,
  Vector = 11,
  Rotator = 12,
  Str = 13,
  Map = 14,
  FixedArray = 15,
};

struct Property {
  Name name;

  union {
    std::uint8_t info;
    struct {
      PropertyType type : 4;
      std::uint8_t size_type : 3;
      std::uint8_t is_array : 1;
    };
  };

  Name struct_name;
  std::uint32_t size;
  ArrayIndex array_index;
  Index array_size;

  union {
    std::uint8_t uint8_t_value;
    std::int32_t int32_t_value;
    float float_value;
    Index index_value;
    Vector vector_value;
    Rotator rotator_value;
  };

  std::vector<std::uint8_t> data_value;
  std::vector<std::unordered_map<std::string, Property>> subproperties;

  auto bool_value() const -> bool;

  auto subproperty(const std::string &name, std::size_t index = 0) const
      -> Property;

  friend auto operator<<(std::ostream &output, const Property &property)
      -> std::ostream &;
};

} // namespace unreal

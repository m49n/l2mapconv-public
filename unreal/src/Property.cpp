#include "pch.h"

#include <unreal/Property.h>

namespace unreal {

auto Property::bool_value() const -> bool {
  return type == PropertyType::Bool && is_array == 1;
}

auto Property::subproperty(const std::string &name, std::size_t index) const
    -> Property {

  ASSERT(index < subproperties.size(), "Index out of bounds");
  const auto &map = subproperties[index];
  const auto pair = map.find(name);

  if (pair == map.end()) {
    utils::Log(utils::LOG_WARN, "Unreal")
        << "Can't find property: " << name << std::endl;
    return {};
  }

  return pair->second;
}

} // namespace unreal

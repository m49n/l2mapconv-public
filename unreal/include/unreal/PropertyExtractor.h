#pragma once

#include "Property.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace unreal {

class Archive;

class PropertyExtractor {
public:
  explicit PropertyExtractor(Archive &archive) : m_archive{archive} {}

  auto extract_properties() const -> std::vector<Property>;

private:
  Archive &m_archive;

  void deserialize(Property &property) const;
  auto extract_size(std::uint8_t size_type) const -> std::uint32_t;
  auto extract_properties_map() const
      -> std::unordered_map<std::string, Property>;
};

} // namespace unreal

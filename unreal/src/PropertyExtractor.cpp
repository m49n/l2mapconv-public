#include "pch.h"

#include <unreal/Archive.h>
#include <unreal/Name.h>
#include <unreal/PropertyExtractor.h>

namespace unreal {

auto PropertyExtractor::extract_properties() const -> std::vector<Property> {
  std::vector<Property> properties;

  while (true) {
    Property property{};
    deserialize(property);

    if (property.name == Name::NONE) {
      break;
    }

    properties.push_back(std::move(property));
  }

  return properties;
}

void PropertyExtractor::deserialize(Property &property) const {
  m_archive >> property.name;

  if (property.name == Name::NONE) {
    return;
  }

  m_archive >> property.info;

  if (property.type == PropertyType::Struct) {
    m_archive >> property.struct_name;
  }

  property.size = extract_size(property.size_type);

  if (property.type == PropertyType::Bool) {
    return;
  }

  if (property.is_array) {
    m_archive >> property.array_index;
  }

  std::istream &input = m_archive;

  switch (property.type) {
  case PropertyType::Byte: {
    m_archive >> property.uint8_t_value;
    break;
  }
  case PropertyType::Int: {
    m_archive >> property.int32_t_value;
    break;
  }
  case PropertyType::Float: {
    m_archive >> property.float_value;
    break;
  }
  case PropertyType::Object:
  case PropertyType::Name: {
    m_archive >> property.index_value;
    break;
  }
  case PropertyType::Array: {
    const auto start_position = input.tellg();
    m_archive >> property.array_size;
    const auto size_size = input.tellg() - start_position;
    const auto array_size = property.size - size_size;

    if (property.name == "Materials") {
      property.properties.reserve(property.array_size);
      const auto array_start_position = input.tellg();

      for (auto i = 0; i < property.array_size; ++i) {
        property.properties.push_back(extract_properties_map());
      }

      const auto array_end_position = input.tellg();
      ASSERT((array_end_position - array_start_position) == array_size,
             "Unreal", "Invalid property array");
    } else {
      property.data_value.resize(array_size);
      input.read(reinterpret_cast<char *>(property.data_value.data()),
                 array_size);
    }

    break;
  }
  case PropertyType::Struct: {
    if (property.struct_name == "Rotator") {
      m_archive >> property.rotator_value;
    } else if (property.struct_name == "Vector") {
      m_archive >> property.vector_value;
    } else if (property.struct_name == "Color") {
      m_archive >> property.color_value;
    } else if (property.struct_name == "Scale") {
      m_archive >> property.scale_value;
    } else if (property.struct_name == "Plane") {
      m_archive >> property.plane_value;
    } else if (property.struct_name == "TerrainLayer") {
      property.properties.push_back(extract_properties_map());
    } else {
      utils::Log(utils::LOG_DEBUG, "Unreal")
          << "Skipping struct: " << property.struct_name << std::endl;
      input.seekg(property.size, std::ios::cur);
    }

    break;
  }
  case PropertyType::Vector: {
    m_archive >> property.vector_value;
    break;
  }
  case PropertyType::Rotator: {
    m_archive >> property.rotator_value;
    break;
  }
  default: {
    utils::Log(utils::LOG_DEBUG, "Unreal")
        << "Skipping property type: " << static_cast<int>(property.type)
        << std::endl;
    input.seekg(property.size, std::ios::cur);
  }
  }
}

auto PropertyExtractor::extract_size(std::uint8_t size_type) const
    -> std::uint32_t {

  std::uint32_t size = 0;

  switch (size_type) {
  case 0: {
    size = 1;
    break;
  }
  case 1: {
    size = 2;
    break;
  }
  case 2: {
    size = 4;
    break;
  }
  case 3: {
    size = 12;
    break;
  }
  case 4: {
    size = 16;
    break;
  }
  case 5: {
    m_archive >> extract<llvm::ulittle8_t>(size);
    break;
  }
  case 6: {
    m_archive >> extract<llvm::ulittle16_t>(size);
    break;
  }
  case 7: {
    m_archive >> size;
    break;
  }
  default: {
    ASSERT(false, "Unreal", "Unknown property size type: " << size_type);
  }
  }

  return size;
}

auto PropertyExtractor::extract_properties_map() const
    -> std::unordered_map<std::string, Property> {

  std::unordered_map<std::string, Property> map;
  const auto properties = extract_properties();

  for (const auto &property : properties) {
    const auto name = std::string{property.name};

    if (map.find(name) == map.end()) {
      utils::Log(utils::LOG_DEBUG, "Unreal")
          << "Property already exists: " << name << std::endl;
    }

    map[name] = property;
  }

  return map;
}

} // namespace unreal

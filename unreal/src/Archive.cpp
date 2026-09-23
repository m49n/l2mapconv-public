#include "pch.h"

#include <unreal/Archive.h>
#include <unreal/ArchiveLoader.h>
#include <unordered_set>
#include <stdexcept>

namespace unreal {
auto Archive::remaining() -> std::streamoff {
  const auto position = m_input.tellg();
  const auto end = m_read_limit >= 0 ? m_read_limit : size();
  if (position < 0 || position > end) throw std::runtime_error("Unreal stream outside serialized object");
  return end - position;
}
void Archive::require_bytes(std::size_t count) {
  if (m_read_limit >= 0 && count > static_cast<std::uint64_t>(remaining()))
    throw std::runtime_error("Read exceeds Unreal object's serial range");
}
auto Archive::object_reference(Index index) const -> AssetReference {
  if (index == 0) return {};
  AssetReference result{std::string{name}, {}, {}};
  std::vector<std::string> parts;
  std::unordered_set<std::int32_t> seen;
  auto current = index.value;
  while (current != 0) {
    if (!seen.insert(current).second || seen.size() > export_map.size() + import_map.size())
      throw std::runtime_error("Cyclic Unreal object outer chain");
    const auto offset = current < 0 ? -static_cast<std::int64_t>(current) - 1 : static_cast<std::int64_t>(current) - 1;
    std::string leaf, type;
    std::int32_t outer = 0;
    if (current < 0) {
      if (static_cast<std::uint64_t>(offset) >= import_map.size()) throw std::runtime_error("Unreal import index out of range");
      const auto& item = import_map[static_cast<std::size_t>(offset)];
      leaf = item.object_name; type = item.class_name; outer = item.package_index;
    } else {
      if (static_cast<std::uint64_t>(offset) >= export_map.size()) throw std::runtime_error("Unreal export index out of range");
      const auto& item = export_map[static_cast<std::size_t>(offset)];
      leaf = item.object_name; type = item.class_name; outer = item.package_index;
    }
    if (seen.size() == 1) result.class_name = type;
    if (current < 0 && outer == 0 && type == "Package") result.package = leaf;
    else parts.push_back(leaf);
    current = outer;
  }
  for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
    if (!result.object_path.empty()) result.object_path += '.';
    result.object_path += *it;
  }
  return result;
}

Archive::Archive(const std::string &name, std::stringstream input,
                 const ArchiveLoader &archive_loader)
    : object_loader{*this, archive_loader}, property_extractor{*this},
      name{m_name_table.name(name)}, m_input{std::move(input)} {

  *this >> header;

  m_input.seekg(header.name_offset);
  for (auto i = 0; i < header.name_count; ++i) {
    std::string name;
    std::uint32_t flags = 0;
    *this >> name >> flags;
    name_map.emplace_back(m_name_table.name(name));
  }

  m_input.seekg(header.import_offset);
  for (auto i = 0; i < header.import_count; ++i) {
    ObjectImport object_import{};
    *this >> object_import;
    import_map.push_back(object_import);
  }

  m_input.seekg(header.export_offset);
  for (auto i = 0; i < header.export_count; ++i) {
    ObjectExport object_export{};
    *this >> object_export;
    export_map.push_back(std::move(object_export));
  }
}

auto Archive::object_name(Index index) const -> Name {
  if (index < 0) {
    ASSERT(static_cast<std::size_t>(-index) <= import_map.size(), "Unreal",
           "Index out of import_map bounds");
    return import_map[-index - 1].object_name;
  }

  if (index > 0) {
    ASSERT(static_cast<std::size_t>(index) <= export_map.size(), "Unreal",
           "Index out of export_map bounds");
    return export_map[index - 1].object_name;
  }

  return m_name_table.none_name();
}

auto Archive::operator>>(PackageHeader &header) -> Archive & {
  *this >> header.magic;
  ASSERT(header.magic == PackageHeader::PACKAGE_MAGIC, "Unreal",
         "Lineage 2 package magic must be equal to "
             << PackageHeader::PACKAGE_MAGIC);

  *this >> header.file_version >> header.license_version;
  *this >> header.flags;
  *this >> header.name_count >> header.name_offset;
  *this >> header.export_count >> header.export_offset;
  *this >> header.import_count >> header.import_offset;
  *this >> header.guid;
  *this >> header.generation_count;

  header.generations.reserve(header.generation_count);

  for (auto i = 0; i < header.generation_count; ++i) {
    GenerationInfo generation{};
    *this >> generation;
    header.generations.push_back(generation);
  }

  return *this;
}

auto Archive::operator>>(GUID &guid) -> Archive & {
  *this >> guid.A >> guid.B >> guid.C >> guid.D;
  return *this;
}

auto Archive::operator>>(GenerationInfo &generation) -> Archive & {
  *this >> generation.export_count >> generation.name_count;
  return *this;
}

auto Archive::operator>>(Name &name) -> Archive & {
  Index index{};
  *this >> index;

  if (static_cast<std::size_t>(index) < name_map.size()) {
    name = name_map[index];
  } else {
    name = m_name_table.none_name();
  }

  return *this;
}

auto Archive::operator>>(Index &index) -> Archive & {
  std::uint8_t byte = 0;
  *this >> byte;

  const auto negative = (byte & (1 << 7)) != 0;
  auto value = byte & 0x3f;

  if ((byte & (1 << 6)) != 0) {
    auto shift = 6;

    do {
      auto data = 0;
      *this >> byte;
      data = byte & 0x7f;
      data <<= shift;
      value |= data;
      shift += 7;
    } while (((byte & (1 << 7)) != 0) && (shift < 32));
  }

  if (negative) {
    value = -value;
  }

  index.value = value;
  return *this;
}

auto Archive::operator>>(ObjectImport &object_import) -> Archive & {
  *this >> object_import.class_package >> object_import.class_name >>
      object_import.package_index >> object_import.object_name;
  return *this;
}

auto Archive::operator>>(ObjectExport &object_export) -> Archive & {
  Index class_index{};
  Index super_index{};

  *this >> class_index >> super_index >> object_export.package_index >>
      object_export.object_name >> object_export.object_flags >>
      object_export.serial_size;

  object_export.class_name = object_name(class_index);
  object_export.super_name = object_name(super_index);

  if (object_export.serial_size > 0) {
    *this >> object_export.serial_offset;
  }

  return *this;
}

auto Archive::operator>>(char &value) -> Archive & {
  require_bytes(1);
  m_input >> value;
  return *this;
}

auto Archive::operator>>(float &value) -> Archive & {
  require_bytes(4);
  m_input.read(reinterpret_cast<char *>(&value), sizeof(value));
  return *this;
}

auto Archive::operator>>(bool &value) -> Archive & {
  require_bytes(1);
  *this >> extract<llvm::little8_t>(value);
  return *this;
}

auto Archive::operator>>(std::string &value) -> Archive & {
  if (m_read_limit >= 0) {
    Index count{}; *this >> count;
    if (count.value < 0 || count.value > 16 * 1024 * 1024) throw std::runtime_error("Invalid bounded Unreal string size");
    require_bytes(static_cast<std::size_t>(count.value));
    value.resize(static_cast<std::size_t>(count.value));
    m_input.read(value.data(), count.value);
    if (!value.empty()) value.pop_back();
    return *this;
  }
  *this >> extract_array<Index, llvm::little8_t>(value);

  if (!value.empty()) {
    value.pop_back();
  }

  return *this;
}

auto Archive::operator>>(std::int8_t &value) -> Archive & {
  require_bytes(1);
  *this >> extract<llvm::little8_t>(value);
  return *this;
}

auto Archive::operator>>(std::int16_t &value) -> Archive & {
  require_bytes(2);
  *this >> extract<llvm::little16_t>(value);
  return *this;
}

auto Archive::operator>>(std::int32_t &value) -> Archive & {
  require_bytes(4);
  *this >> extract<llvm::little32_t>(value);
  return *this;
}

auto Archive::operator>>(std::int64_t &value) -> Archive & {
  require_bytes(8);
  *this >> extract<llvm::little64_t>(value);
  return *this;
}

auto Archive::operator>>(std::uint8_t &value) -> Archive & {
  require_bytes(1);
  *this >> extract<llvm::ulittle8_t>(value);
  return *this;
}

auto Archive::operator>>(std::uint16_t &value) -> Archive & {
  require_bytes(2);
  *this >> extract<llvm::ulittle16_t>(value);
  return *this;
}

auto Archive::operator>>(std::uint32_t &value) -> Archive & {
  require_bytes(4);
  *this >> extract<llvm::ulittle32_t>(value);
  return *this;
}

auto Archive::operator>>(std::uint64_t &value) -> Archive & {
  require_bytes(8);
  *this >> extract<llvm::ulittle64_t>(value);
  return *this;
}

void Archive::dump(int line_count, int line_length) {
  std::istream &input = *this;
  const auto offset = input.tellg();

  std::cout << std::endl;
  std::cout << "Package: " << name << std::endl;
  std::cout << "File Version: " << header.file_version << std::endl;
  std::cout << "License Version: " << header.license_version << std::endl;
  std::cout << "Offset: " << offset << std::endl;

  utils::dump(input, line_count, line_length);
}

} // namespace unreal

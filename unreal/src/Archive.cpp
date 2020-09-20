#include "pch.h"

#include <unreal/Archive.h>
#include <unreal/ArchiveLoader.h>
#include <unreal/ExtractionHelpers.h>

namespace unreal {

Archive::Archive(const ArchiveLoader &archive_loader, Name name,
                 std::stringstream input, NameTable &name_table)
    : object_loader{*this, archive_loader}, property_extractor{*this},
      name{name}, m_input{std::move(input)}, m_name_table{name_table} {

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
  auto value = byte & 0x3F;

  if ((byte & (1 << 6)) != 0) {
    auto shift = 6;

    do {
      auto data = 0;
      *this >> byte;
      data = byte & 0x7F;
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
  m_input >> value;
  return *this;
}

auto Archive::operator>>(float &value) -> Archive & {
  m_input.read(reinterpret_cast<char *>(&value), sizeof(value));
  return *this;
}

auto Archive::operator>>(bool &value) -> Archive & {
  *this >> extract<llvm::little8_t>(value);
  return *this;
}

auto Archive::operator>>(std::string &value) -> Archive & {
  *this >> extract_array<Index, llvm::little8_t>(value);

  if (!value.empty()) {
    value.pop_back();
  }

  return *this;
}

auto Archive::operator>>(std::int8_t &value) -> Archive & {
  *this >> extract<llvm::little8_t>(value);
  return *this;
}

auto Archive::operator>>(std::int16_t &value) -> Archive & {
  *this >> extract<llvm::little16_t>(value);
  return *this;
}

auto Archive::operator>>(std::int32_t &value) -> Archive & {
  *this >> extract<llvm::little32_t>(value);
  return *this;
}

auto Archive::operator>>(std::int64_t &value) -> Archive & {
  *this >> extract<llvm::little64_t>(value);
  return *this;
}

auto Archive::operator>>(std::uint8_t &value) -> Archive & {
  *this >> extract<llvm::ulittle8_t>(value);
  return *this;
}

auto Archive::operator>>(std::uint16_t &value) -> Archive & {
  *this >> extract<llvm::ulittle16_t>(value);
  return *this;
}

auto Archive::operator>>(std::uint32_t &value) -> Archive & {
  *this >> extract<llvm::ulittle32_t>(value);
  return *this;
}

auto Archive::operator>>(std::uint64_t &value) -> Archive & {
  *this >> extract<llvm::ulittle64_t>(value);
  return *this;
}

} // namespace unreal

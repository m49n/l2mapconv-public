#pragma once

#include "ExtractionHelpers.h"
#include "Index.h"
#include "Name.h"
#include "NameTable.h"
#include "ObjectLoader.h"
#include "PropertyExtractor.h"

#include <utils/NonCopyable.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace unreal {

class Object;
class ArchiveLoader;

struct ObjectImport {
  Name class_package;
  Name class_name;
  std::int32_t package_index;
  Name object_name;
};

struct ObjectExport {
  Name class_name;
  Name super_name;
  std::int32_t package_index;
  Name object_name;
  std::uint32_t object_flags;
  Index serial_size;
  Index serial_offset;

  std::shared_ptr<Object> object;
};

struct GUID {
  std::uint32_t A, B, C, D;
};

struct GenerationInfo {
  std::int32_t export_count;
  std::int32_t name_count;
};

enum PackageFlags {
  PKG_AllowDownload = 0x0001,  // Allow downloading package.
  PKG_ClientOptional = 0x0002, // Purely optional for clients.
  PKG_ServerSideOnly = 0x0004, // Only needed on the server side.
  PKG_BrokenLinks = 0x0008,    // Loaded from linker with broken import links.
  PKG_Unsecure = 0x0010,       // Not trusted.
  PKG_Need = 0x8000,           // Client needs to download this package.
};

struct PackageHeader {
  static constexpr std::int32_t PACKAGE_MAGIC = 0x9E2A83C1;

  std::int32_t magic;
  std::int16_t file_version;
  std::int16_t license_version;
  std::uint32_t flags;
  std::int32_t name_count, name_offset;
  std::int32_t export_count, export_offset;
  std::int32_t import_count, import_offset;
  GUID guid;
  std::int32_t generation_count;
  std::vector<GenerationInfo> generations;
};

class Archive : public utils::NonCopyable {
public:
  ObjectLoader object_loader;
  PropertyExtractor property_extractor;

  const Name name;

  PackageHeader header;

  std::vector<Name> name_map;
  std::vector<ObjectImport> import_map;
  std::vector<ObjectExport> export_map;

  explicit Archive(const ArchiveLoader &archive_loader, Name name,
                   std::stringstream input, NameTable &name_table);

  Archive(Archive &&other)
      : object_loader{other.object_loader},
        property_extractor{other.property_extractor}, name{std::move(
                                                          other.name)},
        header{std::move(other.header)}, name_map{std::move(other.name_map)},
        import_map{std::move(other.import_map)}, export_map{std::move(
                                                     other.export_map)},
        m_input{std::move(other.m_input)}, m_name_table{other.m_name_table} {}

  operator std::istream &() { return m_input; }

  auto object_name(Index index) const -> Name;

  auto operator>>(PackageHeader &header) -> Archive &;
  auto operator>>(GUID &guid) -> Archive &;
  auto operator>>(GenerationInfo &generation) -> Archive &;
  auto operator>>(Name &name) -> Archive &;
  auto operator>>(ObjectImport &object_import) -> Archive &;
  auto operator>>(ObjectExport &object_export) -> Archive &;
  auto operator>>(Index &index) -> Archive &;

  auto operator>>(float &value) -> Archive &;
  auto operator>>(char &value) -> Archive &;
  auto operator>>(bool &value) -> Archive &;
  auto operator>>(std::string &value) -> Archive &;

  auto operator>>(std::int8_t &value) -> Archive &;
  auto operator>>(std::int16_t &value) -> Archive &;
  auto operator>>(std::int32_t &value) -> Archive &;
  auto operator>>(std::int64_t &value) -> Archive &;
  auto operator>>(std::uint8_t &value) -> Archive &;
  auto operator>>(std::uint16_t &value) -> Archive &;
  auto operator>>(std::uint32_t &value) -> Archive &;
  auto operator>>(std::uint64_t &value) -> Archive &;

  template <typename T> auto operator>>(std::vector<T> &vector) -> Archive & {
    *this >> extract_array<Index, T>(vector);
    return *this;
  }

  void dump(int lines = 64, int line_length = 24);

  friend auto operator<<(std::ostream &output, const Archive &archive)
      -> std::ostream &;

private:
  std::stringstream m_input;
  NameTable &m_name_table;
};

} // namespace unreal

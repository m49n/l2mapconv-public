#pragma once

#include "Index.h"
#include "AssetReference.h"
#include "Name.h"
#include "NameTable.h"
#include "ObjectLoader.h"
#include "PropertyExtractor.h"

#include <utils/ExtractionHelpers.h>
#include <utils/NonCopyable.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>

namespace unreal {

using namespace utils;

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
  PKG_AllowDownload = 0x0001,  // Allow downloading package
  PKG_ClientOptional = 0x0002, // Purely optional for clients
  PKG_ServerSideOnly = 0x0004, // Only needed on the server side
  PKG_BrokenLinks = 0x0008,    // Loaded from linker with broken import links
  PKG_Unsecure = 0x0010,       // Not trusted
  PKG_Need = 0x8000,           // Client needs to download this package
};

struct PackageHeader {
  static constexpr std::int32_t PACKAGE_MAGIC = 0x9e2a83c1;

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
private:
  mutable NameTable m_name_table;

public:
  const ObjectLoader object_loader;
  const PropertyExtractor property_extractor;

  const Name name;

  PackageHeader header;

  mutable std::vector<Name> name_map;
  mutable std::vector<ObjectImport> import_map;
  mutable std::vector<ObjectExport> export_map;

  explicit Archive(const std::string &name, std::stringstream input,
                   const ArchiveLoader &archive_loader);

  Archive(Archive &&other)
      : object_loader{other.object_loader},
        property_extractor{other.property_extractor}, name{std::move(
                                                          other.name)},
        header{std::move(other.header)}, name_map{std::move(other.name_map)},
        import_map{std::move(other.import_map)},
        export_map{std::move(other.export_map)}, m_input{std::move(
                                                     other.m_input)} {}

  operator std::istream &() { return m_input; }

  auto object_name(Index index) const -> Name;
  auto object_reference(Index index) const -> AssetReference;
  auto size() const -> std::streamoff { return static_cast<std::streamoff>(m_input.view().size()); }
  void require_bytes(std::size_t count);
  auto remaining() -> std::streamoff;
  class ReadLimit {
  public:
    ReadLimit(Archive& archive, std::streamoff end) : m_archive(archive), m_previous(archive.m_read_limit) { archive.m_read_limit = end; }
    ~ReadLimit() { m_archive.m_read_limit = m_previous; }
    ReadLimit(const ReadLimit&) = delete;
  private:
    Archive& m_archive;
    std::streamoff m_previous;
  };

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
    if (m_read_limit >= 0) {
      Index count{}; *this >> count;
      if (count.value < 0 || count.value > 65536 || count.value > remaining())
        throw std::runtime_error("Invalid bounded Unreal array count");
      vector.reserve(static_cast<std::size_t>(count.value));
      for (int i = 0; i < count.value; ++i) { T item{}; *this >> item; vector.push_back(std::move(item)); }
      return *this;
    }
    *this >> extract_array<Index, T>(vector);
    return *this;
  }

  template <typename T>
  void load_objects(const std::string &class_name,
                    std::vector<std::shared_ptr<T>> &objects) const {

    for (auto &object_export : export_map) {
      if (object_export.class_name == class_name) {
        objects.push_back(std::dynamic_pointer_cast<T>(
            object_loader.export_object(object_export)));
      }
    }
  }

  void dump(int line_count = 32, int line_length = 16);

  friend auto operator<<(std::ostream &output, const Archive &archive)
      -> std::ostream &;

private:
  std::stringstream m_input;
  std::streamoff m_read_limit{-1};
};

} // namespace unreal

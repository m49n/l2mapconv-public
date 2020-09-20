#pragma once

#include "Index.h"

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace unreal {

class Archive;
class ArchiveLoader;
class Object;

struct ObjectExport;

class ObjectLoader {
public:
  explicit ObjectLoader(Archive &archive, const ArchiveLoader &archive_loader)
      : m_archive{archive}, m_archive_loader{archive_loader} {}

  auto load_object(std::string_view name) const -> std::shared_ptr<Object>;
  auto load_object(Index index) const -> std::shared_ptr<Object>;

  template <typename T>
  void load_objects(const std::string &class_name,
                    std::vector<std::shared_ptr<T>> &objects) const;

  friend auto operator<<(std::ostream &output,
                         const ObjectLoader &object_loader) -> std::ostream &;

public:
  Archive &m_archive;
  const ArchiveLoader &m_archive_loader;

  auto export_object(ObjectExport &object_export) const
      -> std::shared_ptr<Object>;
};

#include "Archive.h"

template <typename T>
void ObjectLoader::load_objects(
    const std::string &class_name,
    std::vector<std::shared_ptr<T>> &objects) const {

  for (auto &object_export : m_archive.export_map) {
    if (object_export.class_name == class_name) {
      objects.push_back(
          std::dynamic_pointer_cast<T>(export_object(object_export)));
    }
  }
}

} // namespace unreal

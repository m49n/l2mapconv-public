#pragma once

#include "Index.h"
#include "AssetReference.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace unreal {

class Archive;
class ArchiveLoader;
class Object;

struct ObjectImport;
struct ObjectExport;

class ObjectLoader {
public:
  explicit ObjectLoader(Archive &archive, const ArchiveLoader &archive_loader)
      : m_archive{archive}, m_archive_loader{archive_loader} {}

  auto load_object(const AssetReference& reference) const -> std::shared_ptr<Object>;
  auto load_object(Index index) const -> std::shared_ptr<Object>;
  auto reference(Index index) const -> AssetReference;

  auto export_object(ObjectExport &object_export) const
      -> std::shared_ptr<Object>;

  friend auto operator<<(std::ostream &output,
                         const ObjectLoader &object_loader) -> std::ostream &;

private:
  Archive &m_archive;
  const ArchiveLoader &m_archive_loader;
};

} // namespace unreal

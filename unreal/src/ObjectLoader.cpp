#include "pch.h"

#include <unreal/Actor.h>
#include <unreal/Archive.h>
#include <unreal/ArchiveLoader.h>
#include <unreal/Level.h>
#include <unreal/Material.h>
#include <unreal/Object.h>
#include <unreal/ObjectLoader.h>
#include <unreal/StaticMesh.h>
#include <unreal/Terrain.h>

namespace unreal {

auto ObjectLoader::load_object(const ObjectImport &import) const
    -> std::shared_ptr<Object> {

  for (auto &object_export : m_archive.export_map) {
    if (object_export.object_name == import.object_name &&
        object_export.class_name == import.class_name &&
        object_export.class_name != "Package") {
      return export_object(object_export);
    }
  }

  utils::Log(utils::LOG_WARN, "Unreal")
      << "Can't find object: " << import.object_name << std::endl;
  return nullptr;
}

auto ObjectLoader::load_object(Index index) const -> std::shared_ptr<Object> {
  ASSERT(index != 0, "Unreal", "Index can't be equal to zero");

  if (index < 0) {
    ASSERT(static_cast<std::size_t>(-index) <= m_archive.import_map.size(),
           "Unreal", "Index out of import_map bounds");
    auto &import = m_archive.import_map[-index - 1];

    ASSERT(import.package_index != 0, "Unreal",
           "Package index can't be equal to zero");
    auto *package_import = &import;

    do {
      ASSERT(static_cast<std::size_t>(-package_import->package_index) <=
                 m_archive.import_map.size(),
             "Unreal", "Package index out of import_map bounds");
      package_import =
          &m_archive.import_map[-package_import->package_index - 1];
    } while (package_import->package_index != 0);

    auto *archive =
        m_archive_loader.load_archive(std::string{package_import->object_name});

    if (archive == nullptr) {
      return nullptr;
    }

    return archive->object_loader.load_object(import);
  }

  if (index > 0) {
    ASSERT(static_cast<std::size_t>(index) <= m_archive.export_map.size(),
           "Unreal", "Index out of export_map bounds");

    return export_object(m_archive.export_map[index - 1]);
  }

  return nullptr;
}

auto ObjectLoader::export_object(ObjectExport &object_export) const
    -> std::shared_ptr<Object> {

  if (object_export.object != nullptr) {
    return object_export.object;
  }

  std::shared_ptr<Object> object;

  if (object_export.class_name == "Model") {
    object = std::make_shared<Model>(m_archive);
  } else if (object_export.class_name == "Texture") {
    object = std::make_shared<Texture>(m_archive);
  } else if (object_export.class_name == "FinalBlend") {
    object = std::make_shared<FinalBlend>(m_archive);
  } else if (object_export.class_name == "Shader") {
    object = std::make_shared<Shader>(m_archive);
  } else if (object_export.class_name == "StaticMesh") {
    object = std::make_shared<StaticMesh>(m_archive);
  } else if (object_export.class_name == "TerrainInfo") {
    object = std::make_shared<TerrainInfoActor>(m_archive);
  } else if (object_export.class_name == "Level") {
    object = std::make_shared<Level>(m_archive);
  } else if (object_export.class_name == "Brush") {
    object = std::make_shared<BrushActor>(m_archive);
  } else if (object_export.class_name == "BlockingVolume") {
    object = std::make_shared<BlockingVolumeActor>(m_archive);
  } else if (object_export.class_name == "WaterVolume") {
    object = std::make_shared<WaterVolumeActor>(m_archive);
  } else if (object_export.class_name == "StaticMeshActor" ||
             object_export.class_name == "MovableStaticMeshActor" ||
             object_export.class_name == "L2MovableStaticMeshActor") {
    object = std::make_shared<StaticMeshActor>(m_archive);
  } else {
    utils::Log(utils::LOG_WARN, "Unreal")
        << "Unsupported object type: " << object_export.class_name << std::endl;
    object = std::make_shared<Object>(m_archive);
  }

  object->name = object_export.object_name;
  object->flags = object_export.object_flags;

  if (object_export.serial_size > 0) {
    static_cast<std::istream &>(m_archive).seekg(
        object_export.serial_offset.value);
  }

  object->deserialize();

  object_export.object = object;
  return object_export.object;
}

} // namespace unreal

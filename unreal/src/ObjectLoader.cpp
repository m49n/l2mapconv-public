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
#include <stdexcept>

namespace unreal {
auto ObjectLoader::reference(Index index) const -> AssetReference {
  return m_archive.object_reference(index);
}

auto ObjectLoader::load_object(const AssetReference& reference) const
    -> std::shared_ptr<Object> {

  for (std::size_t i = 0; i < m_archive.export_map.size(); ++i) {
    auto& object_export = m_archive.export_map[i];
    const auto leaf = std::string_view(reference.object_path).substr(reference.object_path.find_last_of('.') + 1);
    if (object_export.object_name == leaf && object_export.class_name == reference.class_name &&
        object_export.class_name != "Package") {
      const auto candidate = m_archive.object_reference(Index{static_cast<std::int32_t>(i + 1)});
      if (candidate.package == reference.package && candidate.object_path == reference.object_path)
        return export_object(object_export);
    }
  }

  utils::Log(utils::LOG_WARN, "Unreal")
      << "Can't find object: " << reference.package << '.' << reference.object_path << std::endl;
  return nullptr;
}

auto ObjectLoader::load_object(Index index) const -> std::shared_ptr<Object> {
  if (index == 0) throw std::runtime_error("Null Unreal object index");
  const auto identity = m_archive.object_reference(index); // validates the complete outer chain

  if (index < 0) {
    const auto *archive = m_archive_loader.load_archive(identity.package);

    if (archive == nullptr) {
      return nullptr;
    }

    return archive->object_loader.load_object(identity);
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
  } else if (object_export.class_name == "Palette") {
    object = std::make_shared<Palette>(m_archive);
  } else if (object_export.class_name == "ConstantColor") {
    object = std::make_shared<ConstantColor>(m_archive);
  } else if (object_export.class_name == "TexModifier" ||
             object_export.class_name == "TexPanner" ||
             object_export.class_name == "TexPannerTriggered" ||
             object_export.class_name == "TexOscillator" ||
             object_export.class_name == "TexOscillatorTriggered" ||
             object_export.class_name == "TexRotator" ||
             object_export.class_name == "TexCoordSource" ||
             object_export.class_name == "TexScaler") {
    object = std::make_shared<Modifier>(m_archive);
  } else if (object_export.class_name == "Combiner") {
    object = std::make_shared<Combiner>(m_archive);
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
  const auto item = std::find_if(m_archive.export_map.begin(), m_archive.export_map.end(),
      [&](const auto& entry) { return &entry == &object_export; });
  if (item == m_archive.export_map.end()) throw std::runtime_error("Export does not belong to archive");
  object->m_reference = m_archive.object_reference(Index{static_cast<std::int32_t>(item - m_archive.export_map.begin() + 1)});
  const auto begin = static_cast<std::streamoff>(object_export.serial_offset.value);
  const auto size = static_cast<std::streamoff>(object_export.serial_size.value);
  if (begin < 0 || size < 0 || begin > m_archive.size() || size > m_archive.size() - begin)
    throw std::runtime_error("Unreal export serial range exceeds archive: " + object->full_name());
  object->m_serial_begin = begin;
  object->m_serial_end = begin + size;

  if (object_export.serial_size > 0) {
    static_cast<std::istream&>(m_archive).clear();
    static_cast<std::istream &>(m_archive).seekg(
        object_export.serial_offset.value);
    if (std::dynamic_pointer_cast<Material>(object) || std::dynamic_pointer_cast<Palette>(object)) {
      Archive::ReadLimit limit(m_archive, object->serial_end());
      object->deserialize();
    } else {
      object->deserialize();
    }
  }
  object_export.object = object;
  return object_export.object;
}

} // namespace unreal

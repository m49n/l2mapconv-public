#include "pch.h"

#include <unreal/Archive.h>
#include <unreal/Index.h>
#include <unreal/Object.h>
#include <unreal/ObjectLoader.h>
#include <unreal/Package.h>
#include <unreal/Property.h>

namespace unreal {

auto operator<<(std::ostream &output, const Index &index) -> std::ostream & {
  return output << index.value;
}

auto operator<<(std::ostream &output, const GUID &guid) -> std::ostream & {
  return output << std::hex << guid.A << "-" << guid.B << "-" << guid.C << "-"
                << guid.D << std::dec;
}

auto operator<<(std::ostream &output, const GenerationInfo &generation)
    -> std::ostream & {
  return output << std::endl
                << "export_count: " << generation.export_count << std::endl
                << "name_count: " << generation.name_count;
}

auto operator<<(std::ostream &output, const PackageHeader &header)
    -> std::ostream & {

  output << std::endl
         << "magic: " << header.magic << std::endl
         << "file_version: " << header.file_version << std::endl
         << "license_version: " << header.license_version << std::endl
         << "flags: " << std::bitset<32>(header.flags).to_string() << std::endl
         << "name_count: " << header.name_count << std::endl
         << "name_offset: " << header.name_offset << std::endl
         << "export_count: " << header.export_count << std::endl
         << "export_offset: " << header.export_offset << std::endl
         << "import_count: " << header.import_count << std::endl
         << "import_offset: " << header.import_offset << std::endl
         << "GUID: " << header.guid << std::endl
         << "generation_count: " << header.generation_count << std::endl
         << std::endl
         << "Generations: ";

  for (const auto &generation : header.generations) {
    output << generation;
  }

  return output;
}

auto operator<<(std::ostream &output, const ObjectImport &object_import)
    -> std::ostream & {

  return output << std::endl
                << "class_package: " << object_import.class_package << std::endl
                << "class_name: " << object_import.class_name << std::endl
                << "package_index: " << object_import.package_index << std::endl
                << "object_name: " << object_import.object_name;
}

auto operator<<(std::ostream &output, const ObjectExport &object_export)
    -> std::ostream & {

  return output << std::endl
                << "class_name: " << object_export.class_name << std::endl
                << "super_name: " << object_export.super_name << std::endl
                << "package_index: " << object_export.package_index << std::endl
                << "object_name: " << object_export.object_name << std::endl
                << "object_flags: "
                << std::bitset<32>(object_export.object_flags) << std::endl
                << "serial_size_index: " << object_export.serial_size
                << std::endl
                << "serial_offset_index: " << object_export.serial_offset;
}

auto operator<<(std::ostream &output, const Archive &archive)
    -> std::ostream & {

  output << std::endl << "Header: " << archive.header << std::endl << std::endl;

  output << "Names: " << std::endl;
  for (const auto &name : archive.name_map) {
    output << name << std::endl;
  }

  output << std::endl << "Imports: ";
  for (const auto &import : archive.import_map) {
    output << import << std::endl;
  }

  output << std::endl << "Exports: ";
  for (const auto &export_ : archive.export_map) {
    output << export_ << std::endl;
  }

  return output;
}

auto operator<<(std::ostream &output, const StateFrame &state_frame)
    -> std::ostream & {

  return output << std::endl
                << "node: " << state_frame.node << std::endl
                << "state_node: " << state_frame.state_node << std::endl
                << "probe_mask: "
                << std::bitset<64>(state_frame.probe_mask).to_string()
                << std::endl
                << "latent_action: " << state_frame.latent_action << std::endl
                << "offset: " << state_frame.offset;
}

auto operator<<(std::ostream &output, const Object &object) -> std::ostream & {
  return output << std::endl
                << "name: " << object.name << std::endl
                << "flags: " << std::bitset<32>(object.flags).to_string()
                << std::endl
                << "state_frame: " << object.state_frame;
}

auto operator<<(std::ostream &output, const Property &property)
    -> std::ostream & {

  return output << std::endl
                << "name: " << property.name << std::endl
                << "type: " << static_cast<int>(property.type) << std::endl
                << "size_type: " << static_cast<int>(property.size_type)
                << std::endl
                << "is_array: " << static_cast<int>(property.is_array)
                << std::endl
                << "struct_name: " << property.struct_name << std::endl
                << "size: " << property.size << std::endl
                << "array_index: " << property.array_index;
}

auto operator<<(std::ostream &output, const ObjectLoader &object_loader)
    -> std::ostream & {

  return output << object_loader.m_archive;
}

auto operator<<(std::ostream &output, const Package &package)
    -> std::ostream & {

  return output << package.m_archive;
}

} // namespace unreal

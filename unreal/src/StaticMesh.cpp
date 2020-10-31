#include "pch.h"

#include <unreal/StaticMesh.h>

namespace unreal {

auto operator>>(Archive &archive, StaticMeshSurface &surface) -> Archive & {
  archive >> surface.unknown >> surface.first_index >>
      surface.min_vertex_index >> surface.max_vertex_index >>
      surface.triangle_count >> surface.triangle_max;
  return archive;
}

auto operator>>(Archive &archive, StaticMeshVertex &vertex) -> Archive & {
  archive >> vertex.location >> vertex.normal;
  return archive;
}

auto operator>>(Archive &archive, StaticMeshVertexStream &stream) -> Archive & {
  archive >> stream.vertices >> stream.revision;
  return archive;
}

auto operator>>(Archive &archive, StaticMeshColorStream &stream) -> Archive & {
  archive >> stream.colors >> stream.revision;
  return archive;
}

auto operator>>(Archive &archive, StaticMeshUV &uv) -> Archive & {
  archive >> uv.u >> uv.v;
  return archive;
}

auto operator>>(Archive &archive, StaticMeshUVStream &stream) -> Archive & {
  archive >> stream.uvs >> stream.coordinate_index >> stream.revision;
  return archive;
}

auto operator>>(Archive &archive, StaticMeshIndexStream &stream) -> Archive & {
  archive >> stream.indices >> stream.revision;
  return archive;
}

void StaticMesh::deserialize() {
  Primitive::deserialize();

  archive >> surfaces >> bounding_box >> vertex_stream >> color_stream >>
      alpha_stream >> uv_stream >> index_stream >> wireframe_index_stream >>
      collision_model;
}

auto StaticMesh::set_property(const Property &property) -> bool {
  if (Primitive::set_property(property)) {
    return true;
  }

  if (property.name == "Materials") {
    for (auto i = 0; i < property.array_size.value; ++i) {
      StaticMeshMaterial material{};

      material.enable_collision =
          property.subproperty("EnableCollision", i).bool_value();
      material.material.from_property(property.subproperty("Material", i),
                                      archive);

      materials.push_back(std::move(material));
    }

    return true;
  }

  if (property.name == "UseSimpleLineCollision") {
    use_simple_line_collision = property.bool_value();
    return true;
  }

  if (property.name == "UseSimpleBoxCollision") {
    use_simple_box_collision = property.bool_value();
    return true;
  }

  if (property.name == "UseSimpleKarmaCollision") {
    use_simple_karma_collision = property.bool_value();
    return true;
  }

  return false;
}

} // namespace unreal

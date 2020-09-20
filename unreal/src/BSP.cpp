#include "pch.h"

#include <unreal/BSP.h>

namespace unreal {

void Primitive::deserialize() {
  Object::deserialize();
  archive >> bounding_box >> bounding_sphere;
}

auto operator>>(Archive &archive, BSPNode &node) -> Archive & {
  archive >> node.plane >> node.zone_mask >> node.flags >>
      node.vertex_pool_index >> node.surface_index >> node.back_index >>
      node.front_index >> node.plane_index >> node.collision_bound >>
      node.render_bound >> node.unknown_point >> node.unknown_id >>
      node.connectivity >> node.visibility >> node.zone[0] >> node.zone[1] >>
      node.vertex_count >> node.leaf[0] >> node.leaf[1];

  // Skip 4 pointers (4*4 bytes) to projected textures.
  static_cast<std::istream &>(archive).seekg(12, std::ios::cur);

  return archive;
}

auto operator>>(Archive &archive, BSPSurface &surface) -> Archive & {
  archive >> surface.material >> surface.polygon_flags >> surface.base_index >>
      surface.normal_index >> surface.u_index >> surface.v_index >>
      surface.brush_polygon >> surface.actor >> surface.plane >>
      surface.nodes[0];

  // TODO: Clarify version.
  if (archive.header.license_version > 20) {
    archive >> surface.nodes[1];
  }

  return archive;
}

auto operator>>(Archive &archive, Vertex &vertex) -> Archive & {
  archive >> vertex.vertex_index >> vertex.side_index;
  return archive;
}

void Model::deserialize() {
  Primitive::deserialize();
  archive >> vectors >> points >> nodes >> surfaces >> vertices;
}

} // namespace unreal

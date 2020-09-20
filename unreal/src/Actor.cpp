#include "pch.h"

#include <unreal/Actor.h>

namespace unreal {

auto Actor::set_property(const Property &property) -> bool {
  if (Object::set_property(property)) {
    return true;
  }

  if (property.name == "Location") {
    location = property.vector_value;
    return true;
  }

  if (property.name == "Rotation") {
    rotation = property.rotator_value;
    return true;
  }

  if (property.name == "DrawScale") {
    draw_scale = property.float_value;
    return true;
  }

  if (property.name == "DrawScale3D") {
    draw_scale_3d = property.vector_value;
    return true;
  }

  if (property.name == "StaticMesh") {
    static_mesh.from_property(property, archive);
    return true;
  }

  if (property.name == "bDeleteMe") {
    delete_me = property.bool_value();
    return true;
  }

  if (property.name == "bHidden") {
    hidden = property.bool_value();
    return true;
  }

  if (property.name == "bCollideActors") {
    collide_actors = property.bool_value();
    return true;
  }

  if (property.name == "bBlockActors") {
    block_actors = property.bool_value();
    return true;
  }

  if (property.name == "bBlockPlayers") {
    block_players = property.bool_value();
    return true;
  }

  if (property.name == "PrePivot") {
    pre_pivot = property.vector_value;
    return true;
  }

  if (property.name == "bBlockNonZeroExtentTraces") {
    block_non_zero_extent_traces = property.bool_value();
    return true;
  }

  if (property.name == "bUseCylinderCollision") {
    use_cylinder_collision = property.bool_value();
    return true;
  }

  if (property.name == "bWorldGeometry") {
    world_geometry = property.bool_value();
    return true;
  }

  return false;
}

auto Actor::position() const -> Vector {
  return {location.x - pre_pivot.x, location.y - pre_pivot.y,
          location.z - pre_pivot.z};
}

auto Actor::scale() const -> Vector {
  return {draw_scale_3d.x * draw_scale, draw_scale_3d.y * draw_scale,
          draw_scale_3d.z * draw_scale};
}

auto BrushActor::set_property(const Property &property) -> bool {
  if (Actor::set_property(property)) {
    return true;
  }

  if (property.name == "Brush") {
    brush.from_property(property, archive);
    return true;
  }

  return false;
}

} // namespace unreal

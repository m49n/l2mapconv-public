#include "pch.h"

#include <unreal/Actor.h>
#include <stdexcept>

namespace unreal {

auto Actor::set_property(const Property &property) -> bool {
  if (property.name == "Skins") {
    if (property.type == PropertyType::Object) {
      const auto slot = property.array_index.value;
      if (slot < 0 || slot >= 256) throw std::runtime_error("Invalid actor skin slot");
      if (skins.size() <= static_cast<std::size_t>(slot)) skins.resize(static_cast<std::size_t>(slot) + 1);
      skins[slot].from_property(property, archive);
      return true;
    }
    if (property.type != PropertyType::Array || property.array_size.value < 0 || property.array_size.value > 256)
      throw std::runtime_error("Invalid actor skin array");
    std::size_t cursor = 0;
    auto byte = [&]() {
      if (cursor >= property.data_value.size()) throw std::runtime_error("Truncated actor skin index");
      return property.data_value[cursor++];
    };
    std::vector<MaterialReference> parsed;
    for (int i = 0; i < property.array_size.value; ++i) {
      const auto first = byte();
      const bool negative = (first & 128) != 0;
      bool more = (first & 64) != 0;
      std::int64_t value = first & 63;
      unsigned shift = 6;
      while (more) {
        if (shift > 27) throw std::runtime_error("Oversized actor skin index");
        const auto next = byte();
        value |= static_cast<std::int64_t>(next & 127) << shift;
        more = (next & 128) != 0; shift += 7;
      }
      if (negative) value = -value;
      if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error("Actor skin index overflow");
      Property reference{}; reference.index_value.value = static_cast<std::int32_t>(value);
      MaterialReference skin; skin.from_property(reference, archive); parsed.push_back(std::move(skin));
    }
    if (cursor != property.data_value.size()) throw std::runtime_error("Trailing actor skin array bytes");
    skins = std::move(parsed);
    return true;
  }
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

#include "pch.h"

#include <unreal/Terrain.h>

namespace unreal {

auto TerrainInfoActor::set_property(const Property &property) -> bool {
  if (InfoActor::set_property(property)) {
    return true;
  }

  if (property.name == "TerrainMap") {
    terrain_map.from_property(property, archive);
    return true;
  }

  if (property.name == "TerrainScale") {
    terrain_scale = property.vector_value;
    return true;
  }

  if (property.name == "QuadVisibilityBitmap") {
    quad_visibility_bitmap.insert(property.data_value);
    return true;
  }

  if (property.name == "EdgeTurnBitmap") {
    edge_turn_bitmap.insert(property.data_value);
    return true;
  }

  if (property.name == "MapX") {
    map_x = property.int32_t_value;
    return true;
  }

  if (property.name == "MapY") {
    map_y = property.int32_t_value;
    return true;
  }

  if (property.name == "Layers") {
    TerrainLayer layer{};

    layer.texture.from_property(property.subproperty("Texture"), archive);
    layer.alpha_map.from_property(property.subproperty("AlphaMap"), archive);
    layer.u_scale = property.subproperty("UScale").float_value;
    layer.v_scale = property.subproperty("VScale").float_value;
    layer.u_pan = property.subproperty("UPan").float_value;
    layer.v_pan = property.subproperty("VPan").float_value;
    layer.texture_map_axis =
        property.subproperty("TextureMapAxis").uint8_t_value;
    layer.texture_rotation =
        property.subproperty("TextureRotation").float_value;

    layers.push_back(std::move(layer));

    return true;
  }

  return false;
}

auto TerrainInfoActor::position() const -> Vector {
  const float width = terrain_map->u_size;
  const float height = terrain_map->v_size;
  const auto scale_ = scale();

  if (!broken_scale()) {
    return {location.x - width / 2.0f * scale_.x,
            location.y - height / 2.0f * scale_.y,
            location.z - 32768.0f * scale_.z};
  }

  return {static_cast<float>(map_x - 20) * width * 128.0f,
          static_cast<float>(map_y - 18) * height * 128.0f, 0.0f};
}

auto TerrainInfoActor::scale() const -> Vector {
  if (!broken_scale()) {
    return {terrain_scale.x, terrain_scale.y, terrain_scale.z / 256.0f};
  }

  return {128.0f, 128.0f, 76.0f / 256.0f};
}

auto TerrainInfoActor::bounding_box() const -> Box {
  const float width = terrain_map->u_size;
  const float height = terrain_map->v_size;
  const auto position_ = position();
  const auto scale_ = scale();

  return {{0.0f, 0.0f, -(16384.0f + position_.z) / scale_.z},
          {width, height, (16384.0f - position_.z) / scale_.z},
          true};
}

auto TerrainInfoActor::broken_scale() const -> bool {
  return terrain_scale.x == 0.0f || terrain_scale.y == 0.0f ||
         terrain_scale.z == 0.0f;
}

} // namespace unreal

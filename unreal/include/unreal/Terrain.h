#pragma once

#include "Actor.h"
#include "Material.h"
#include "ObjectRef.h"
#include "Primitives.h"
#include "Property.h"

#include <utils/Bitset.h>

#include <cstdint>
#include <vector>

namespace unreal {

enum TextureMapAxis {
  TEXMAPAXIS_XY = 0,
  TEXMAPAXIS_XZ = 1,
  TEXMAPAXIS_YZ = 2,
  TEXMAPAXIS_MAX = 3,
};

struct TerrainLayer {
  std::uint8_t texture_map_axis;
  ObjectRef<Texture> texture;
  ObjectRef<Texture> alpha_map;
  float u_scale, v_scale;
  float u_pan, v_pan;
  float texture_rotation;
  Rotator layer_rotation;
  Matrix terrain_matrix;
  float friction;
  float restitution;
  ObjectRef<Texture> layer_weight_map;
};

class TerrainInfoActor : public InfoActor {
public:
  ObjectRef<Texture> terrain_map;
  Vector terrain_scale;
  std::vector<TerrainLayer> layers;
  utils::Bitset quad_visibility_bitmap;
  utils::Bitset edge_turn_bitmap;
  std::int32_t map_x;
  std::int32_t map_y;

  explicit TerrainInfoActor(Archive &archive) : InfoActor{archive} {}

  virtual auto set_property(const Property &property) -> bool override;

  auto position() const -> Vector;
  auto scale() const -> Vector;
  auto bounding_box() const -> Box;
};

} // namespace unreal

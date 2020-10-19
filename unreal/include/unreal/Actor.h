#pragma once

#include "Object.h"
#include "ObjectRef.h"
#include "Primitives.h"
#include "Property.h"
#include "StaticMesh.h"

namespace unreal {

class Actor : public Object {
public:
  Vector location;
  Rotator rotation;
  float draw_scale;
  Vector draw_scale_3d;
  Vector pre_pivot;
  ObjectRef<StaticMesh, ObjectRefConstraint::Optional> static_mesh;
  bool delete_me;
  bool hidden;
  bool collide_actors;
  bool block_actors;
  bool block_players;
  bool block_non_zero_extent_traces;
  bool use_cylinder_collision;
  bool world_geometry;

  explicit Actor(Archive &archive)
      : Object{archive}, location{0.0f, 0.0f, 0.0f}, rotation{0, 0, 0},
        draw_scale{1.0f}, draw_scale_3d{1.0f, 1.0f, 1.0f},
        pre_pivot{0.0f, 0.0f, 0.0f}, delete_me{false}, hidden{false},
        collide_actors{true}, block_actors{true}, block_players{true} {}

  virtual auto set_property(const Property &property) -> bool override;

  auto position() const -> Vector;
  auto scale() const -> Vector;
};

class InfoActor : public Actor {
public:
  explicit InfoActor(Archive &archive) : Actor{archive} {}
};

class BrushActor : public Actor {
public:
  ObjectRef<Model, ObjectRefConstraint::Optional> brush;

  explicit BrushActor(Archive &archive) : Actor{archive} {}

  virtual auto set_property(const Property &property) -> bool override;
};

class StaticMeshActor : public Actor {
public:
  explicit StaticMeshActor(Archive &archive) : Actor{archive} {}
};

class VolumeActor : public BrushActor {
public:
  explicit VolumeActor(Archive &archive) : BrushActor{archive} {}
};

class BlockingVolumeActor : public VolumeActor {
public:
  explicit BlockingVolumeActor(Archive &archive) : VolumeActor{archive} {}
};

class PhysicsVolumeActor : public VolumeActor {
public:
  bool water_volume;

  explicit PhysicsVolumeActor(Archive &archive)
      : VolumeActor{archive}, water_volume{false} {}
};

class WaterVolumeActor : public PhysicsVolumeActor {
public:
  explicit WaterVolumeActor(Archive &archive) : PhysicsVolumeActor{archive} {
    water_volume = true;
  }
};

} // namespace unreal

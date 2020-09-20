#pragma once

#include "Actor.h"
#include "Archive.h"
#include "Object.h"
#include "ObjectRef.h"

#include <cstdint>
#include <string>
#include <vector>

namespace unreal {

struct URL {
  // URL components.
  std::string protocol; // Protocol, i.e. "unreal" or "http".
  std::string host;     // Optional hostname, i.e. "204.157.115.40" or
                        // "unreal.epicgames.com", blank if local.
  std::int32_t port;    // Optional host port.
  std::string map;      // Map name, i.e. "SkyCity", default is "Index".
  std::vector<std::string> options;
  std::string portal;               // Portal to enter through, default is "".

  bool valid; // Whether parsed successfully.

  friend auto operator>>(Archive &archive, URL &url) -> Archive &;
};

enum ReachSpecFlags {
  RS_WALK = 1, // Walking required.
  RS_FLY = 2,  // Flying required.
  RS_SWIM = 4, // Swimming required.
  RS_JUMP = 8, // Jumping required.
  RS_DOOR = 16,
  RS_SPECIAL = 32,
  RS_LADDER = 64,
  RS_PROSCRIBED = 128,
  RS_FORCED = 256,
  RS_PLAYERONLY = 512,
};

struct ReachSpec {
  std::int32_t distance;
  ObjectRef<Actor> start;
  ObjectRef<Actor> end;
  std::int32_t collision_radius;
  std::int32_t collision_height;
  std::int32_t flags;
  bool pruned;

  friend auto operator>>(Archive &archive, ReachSpec &reach_spec) -> Archive &;
};

class Level : public Object {
public:
  std::vector<ObjectRef<Object>> objects;
  URL url;
  std::vector<ReachSpec> reach_specs;
  ObjectRef<Model> model;

  explicit Level(Archive &archive) : Object{archive} {}

  virtual void deserialize() override;
};

} // namespace unreal

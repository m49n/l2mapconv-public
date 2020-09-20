#include "pch.h"

#include <unreal/Level.h>

namespace unreal {

auto operator>>(Archive &archive, URL &url) -> Archive & {
  archive >> url.protocol >> url.host >> url.map >> url.portal >> url.options >>
      url.port >> url.valid;
  return archive;
}

auto operator>>(Archive &archive, ReachSpec &reach_spec) -> Archive & {
  archive >> reach_spec.distance >> reach_spec.start >> reach_spec.end >>
      reach_spec.collision_radius >> reach_spec.collision_height >>
      reach_spec.flags >> reach_spec.pruned;
  return archive;
}

void Level::deserialize() {
  Object::deserialize();

  std::int32_t count1 = 0;
  archive >> count1 >> count1;
  objects.reserve(count1);

  for (auto i = 0; i < count1; ++i) {
    ObjectRef<Object> object_ref{};
    archive >> object_ref;
    objects.push_back(object_ref);
  }

  // TODO: Clarify version.
  if (archive.header.license_version > 20) {
    std::int32_t count2 = 0;
    archive >> count2 >> count2;
    objects.reserve(count1 + count2);

    for (auto i = 0; i < count2; ++i) {
      ObjectRef<Object> object_ref{};
      archive >> object_ref;
      objects.push_back(object_ref);
    }
  }

  archive >> url;

  // Why 2 bytes? In UE bool = 4 bytes (dword), but it doesn't work, so I
  // read only 1 byte for bool (url.valid field) and next 2 bytes of something
  // unknown.
  static_cast<std::istream &>(archive).seekg(2, std::ios::cur);

  archive >> reach_specs >> model;
}

} // namespace unreal

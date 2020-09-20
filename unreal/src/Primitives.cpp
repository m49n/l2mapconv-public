#include "pch.h"

#include <unreal/Archive.h>
#include <unreal/Primitives.h>

namespace unreal {

auto operator>>(Archive &archive, Color &color) -> Archive & {
  archive >> color.r >> color.g >> color.b >> color.a;
  return archive;
}

auto operator>>(Archive &archive, Vector &vector) -> Archive & {
  archive >> vector.x >> vector.y >> vector.z;
  return archive;
}

auto operator>>(Archive &archive, Plane &plane) -> Archive & {
  archive >> plane.x >> plane.y >> plane.z >> plane.w;
  return archive;
}

auto Rotator::vector() const -> Vector {
  return {-roll * (std::numbers::pi_v<float>) / 32768.0f,
          -pitch * (std::numbers::pi_v<float>) / 32768.0f,
          yaw * (std::numbers::pi_v<float>) / 32768.0f};
}

auto operator>>(Archive &archive, Rotator &rotator) -> Archive & {
  archive >> rotator.pitch >> rotator.yaw >> rotator.roll;
  return archive;
}

auto operator>>(Archive &archive, Scale &scale) -> Archive & {
  archive >> scale.scale >> scale.sheer_rate >> scale.sheer_axis;
  return archive;
}

auto operator>>(Archive &archive, Box &box) -> Archive & {
  archive >> box.min >> box.max >> box.is_valid;
  return archive;
}

auto operator>>(Archive &archive, Sphere &sphere) -> Archive & {
  archive >> sphere.location >> sphere.radius;
  return archive;
}

auto operator>>(Archive &archive, Matrix &matrix) -> Archive & {
  archive >> matrix.m[0][0] >> matrix.m[0][1] >> matrix.m[0][2] >>
      matrix.m[0][3] >> matrix.m[1][0] >> matrix.m[1][1] >> matrix.m[1][2] >>
      matrix.m[1][3] >> matrix.m[2][0] >> matrix.m[2][1] >> matrix.m[2][2] >>
      matrix.m[2][3] >> matrix.m[3][0] >> matrix.m[3][1] >> matrix.m[3][2] >>
      matrix.m[3][3];
  return archive;
}

auto operator>>(Archive &archive, Coords &coords) -> Archive & {
  archive >> coords.origin >> coords.x_axis >> coords.y_axis >> coords.z_axis;
  return archive;
}

auto operator>>(Archive &archive, Quat &quat) -> Archive & {
  archive >> quat.x >> quat.y >> quat.z >> quat.w;
  return archive;
}

} // namespace unreal

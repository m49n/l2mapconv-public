#pragma once

#include <cstdint>

namespace unreal {

class Archive;

struct Color {
  std::uint8_t r, g, b, a;

  friend auto operator>>(Archive &archive, Color &color) -> Archive &;
};

struct Vector {
  float x, y, z;

  friend auto operator>>(Archive &archive, Vector &vector) -> Archive &;
};

struct Plane {
  float x, y, z, w;

  friend auto operator>>(Archive &archive, Plane &plane) -> Archive &;
};

struct Rotator {
  std::int32_t pitch; // y: -pitch * pi / 32768.0f
  std::int32_t yaw;   // z: yaw * pi / 32768.0f
  std::int32_t roll;  // x: -roll * pi / 32768.0f

  auto vector() const -> Vector;

  friend auto operator>>(Archive &archive, Rotator &rotator) -> Archive &;
};

// Scaling and sheering info associated with a brush. This is
// easily-manipulated information which is built into a transformation
// matrix later.

enum SheerAxis {
  SHEER_None = 0,
  SHEER_XY = 1,
  SHEER_XZ = 2,
  SHEER_YX = 3,
  SHEER_YZ = 4,
  SHEER_ZX = 5,
  SHEER_ZY = 6,
};

struct Scale {
  Vector scale;
  float sheer_rate;
  std::uint8_t sheer_axis;

  friend auto operator>>(Archive &archive, Scale &scale) -> Archive &;
};

// A rectangular minimum bounding volume.
struct Box {
  Vector min;
  Vector max;
  bool is_valid;

  friend auto operator>>(Archive &archive, Box &box) -> Archive &;
};

struct Sphere {
  Vector location;
  float radius;

  friend auto operator>>(Archive &archive, Sphere &sphere) -> Archive &;
};

struct Matrix {
  float m[4][4];

  friend auto operator>>(Archive &archive, Matrix &matrix) -> Archive &;
};

// A coordinate system matrix.
struct Coords {
  Vector origin;
  Vector x_axis;
  Vector y_axis;
  Vector z_axis;

  friend auto operator>>(Archive &archive, Coords &coords) -> Archive &;
};

struct Quat {
  float x, y, z, w;

  friend auto operator>>(Archive &archive, Quat &quat) -> Archive &;
};

} // namespace unreal

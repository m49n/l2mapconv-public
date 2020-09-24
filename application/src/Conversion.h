#pragma once

#include <unreal/Primitives.h>

#include <math/Box.h>

#include <glm/glm.hpp>

auto to_vec3(const unreal::Vector &vector) -> glm::vec3 {
  return {vector.x, vector.y, vector.z};
}

auto to_box(const unreal::Box &box) -> math::Box {
  return math::Box{to_vec3(box.min), to_vec3(box.max)};
}

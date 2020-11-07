#pragma once

#include "Entity.h"

#include <math/Box.h>

#include <glm/glm.hpp>

#include <vector>

struct Map {
  std::vector<Entity<EntityMesh>> entities;
  glm::vec3 position;
  math::Box bounding_box;

  explicit Map() : entities{}, position{}, bounding_box{} {}
};

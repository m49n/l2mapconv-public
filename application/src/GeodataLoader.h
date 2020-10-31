#pragma once

#include "Entity.h"

#include <rendering/TextureLoader.h>

#include <math/Box.h>

#include <geodata/Loader.h>

#include <glm/glm.hpp>

#include <string>
#include <vector>

class GeodataLoader {
public:
  explicit GeodataLoader() : m_geodata_loader{"geodata"} {}

  auto load_geodata(const std::string &name, const glm::vec3 &position,
                    const math::Box &bounding_box) const
      -> std::vector<Entity<GeodataMesh>>;

private:
  geodata::Loader m_geodata_loader;
};

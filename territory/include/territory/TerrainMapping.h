#pragma once
#include <glm/glm.hpp>
#include <string>
#include <unreal/Terrain.h>
namespace territory {
struct TerrainMapping {
  glm::mat4 world_to_uv{1.f};
  bool verified{};
  std::string evidence;
};
TerrainMapping terrain_mapping(const unreal::TerrainInfoActor &,
                               const unreal::TerrainLayer &);
} // namespace territory

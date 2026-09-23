#pragma once
#include <glm/glm.hpp>
#include <span>
#include <string>
#include <unreal/AssetReference.h>
namespace territory {
enum class WaterEvidence { None, VolumeOnly, Surface, UnsupportedActor };
bool include_water(WaterEvidence, bool enabled);
struct WaterVolumeBounds {
  glm::vec3 min{}, max{};
  std::string source;
};
// A bounded, evidenced P542 material profile AND a matching physical water
// level. This never creates geometry from a volume and never matches a
// substring.
std::string water_surface(const unreal::AssetReference &,
                          std::span<const glm::vec3> polygon,
                          std::span<const WaterVolumeBounds>);
} // namespace territory

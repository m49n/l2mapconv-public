#pragma once
#include "Job.h"
#include "VisualScene.h"
#include <filesystem>
#include <unreal/Actor.h>
namespace territory {
bool visual_bsp_visible(std::uint32_t flags);
// Offline normal zone state. The server can select other seasonal/event states.
inline constexpr int normal_zone_state = 1;
bool visual_actor_visible(const unreal::Actor &,
                          int zone_state = normal_zone_state);
glm::mat4 visual_actor_transform(const unreal::Actor &);
const unreal::MaterialReference &
visual_skin(const unreal::Actor &, const unreal::StaticMesh &, std::size_t slot,
            int zone_state = normal_zone_state);
class VisualSceneLoader {
public:
  explicit VisualSceneLoader(const std::filesystem::path &client)
      : client_(client) {}
  VisualScene load(const std::string &map, const Cancel &cancel = {});

private:
  std::filesystem::path client_;
};
} // namespace territory

#pragma once
#include "Job.h"
#include "VisualScene.h"
#include <filesystem>
#include <unreal/Actor.h>
namespace territory {
bool visual_bsp_visible(std::uint32_t flags);
glm::mat4 visual_actor_transform(const unreal::Actor &);
const unreal::MaterialReference &visual_skin(const unreal::Actor &,
                                             const unreal::StaticMesh &,
                                             std::size_t slot);
class VisualSceneLoader {
public:
  explicit VisualSceneLoader(const std::filesystem::path &client)
      : client_(client) {}
  VisualScene load(const std::string &map, const Cancel &cancel = {});

private:
  std::filesystem::path client_;
};
} // namespace territory

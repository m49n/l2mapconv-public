#pragma once

#include <glm/glm.hpp>

struct CameraMotionInput {
  bool forward{};
  bool backward{};
  bool left{};
  bool right{};
  bool up{};
  bool down{};
  bool fast{};
  bool slow{};
};

auto camera_translation(const CameraMotionInput &input,
                        const glm::vec3 &forward, const glm::vec3 &right,
                        const glm::vec3 &up, float seconds, float base_speed)
    -> glm::vec3;

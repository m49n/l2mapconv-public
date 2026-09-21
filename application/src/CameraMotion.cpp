#include "CameraMotion.h"

#include <algorithm>

auto camera_translation(const CameraMotionInput &input,
                        const glm::vec3 &forward, const glm::vec3 &right,
                        const glm::vec3 &up, float seconds, float base_speed)
    -> glm::vec3 {
  auto direction = glm::vec3{};

  if (input.forward) {
    direction -= forward;
  }
  if (input.backward) {
    direction += forward;
  }
  if (input.left) {
    direction -= right;
  }
  if (input.right) {
    direction += right;
  }
  if (input.up) {
    direction += up;
  }
  if (input.down) {
    direction -= up;
  }

  if (glm::dot(direction, direction) > 0.0f) {
    direction = glm::normalize(direction);
  }

  const auto fast_modifier = input.fast ? 10.0f : 1.0f;
  const auto slow_modifier = input.slow ? 0.2f : 1.0f;
  const auto frame_time = std::clamp(seconds, 0.0f, 0.1f);
  return direction * base_speed * frame_time * fast_modifier * slow_modifier;
}

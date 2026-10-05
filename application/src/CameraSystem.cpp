#include "pch.h"

#include "CameraMotion.h"
#include "CameraSystem.h"

#include <algorithm>
#include <cmath>

CameraSystem::CameraSystem(RenderingContext &rendering_context,
                           WindowContext &window_context, UIContext &ui_context)
    : m_rendering_context{rendering_context}, m_window_context{window_context},
      m_ui_context{ui_context} {}

void CameraSystem::frame_begin(Timestep frame_time) {
  const auto &mouse = m_window_context.mouse;
  const auto &keyboard = m_window_context.keyboard;
  auto &camera = m_rendering_context.camera;
  if (camera.top_view()) return;

  // Rotation
  if (mouse.right) {
    camera.rotate(-mouse.position.dx * m_ui_context.camera.mouse_sensitivity,
                  camera.up());
    // Clamp absolute pitch, not each mouse delta: excess input at a pole
    // must not accumulate, so reversing the mouse responds immediately.
    const auto pitch = std::asin(std::clamp(
        glm::dot(camera.forward(), camera.up()), -1.0f, 1.0f));
    const auto limit = glm::radians(89.0f);
    const auto next_pitch = std::clamp(
        pitch - mouse.position.dy * m_ui_context.camera.mouse_sensitivity,
        -limit, limit);
    camera.rotate(next_pitch - pitch, camera.right());
  }

  // Translation
  const CameraMotionInput input{
      .forward = keyboard.w,
      .backward = keyboard.s,
      .left = keyboard.a,
      .right = keyboard.d,
      .up = keyboard.space,
      .down = keyboard.control,
      .fast = keyboard.shift,
      .slow = keyboard.alt,
  };
  camera.translate(camera_translation(
      input, camera.forward(), camera.right(), camera.up(),
      frame_time.seconds(), m_ui_context.camera.speed));
}

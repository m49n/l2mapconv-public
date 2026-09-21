#include "pch.h"

#include "CameraMotion.h"
#include "CameraSystem.h"

CameraSystem::CameraSystem(RenderingContext &rendering_context,
                           WindowContext &window_context, UIContext &ui_context)
    : m_rendering_context{rendering_context}, m_window_context{window_context},
      m_ui_context{ui_context} {}

void CameraSystem::frame_begin(Timestep frame_time) {
  const auto &mouse = m_window_context.mouse;
  const auto &keyboard = m_window_context.keyboard;
  auto &camera = m_rendering_context.camera;

  // Rotation
  if (mouse.right) {
    camera.rotate(-mouse.position.dx * m_ui_context.camera.mouse_sensitivity,
                  camera.up());
    camera.rotate(-mouse.position.dy * m_ui_context.camera.mouse_sensitivity,
                  camera.right());
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

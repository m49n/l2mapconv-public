#include "pch.h"

#include "CameraSystem.h"

CameraSystem::CameraSystem(RenderingContext &rendering_context,
                           WindowContext &window_context, UIContext &ui_context)
    : m_rendering_context{rendering_context}, m_window_context{window_context},
      m_ui_context{ui_context} {}

void CameraSystem::frame_begin(Timestep frame_time) {
  const auto rotation_speed = 0.002f;
  const auto translation_speed = 3000.0f * frame_time.seconds();

  const auto &mouse = m_window_context.mouse;
  const auto &keyboard = m_window_context.keyboard;
  auto &camera = m_rendering_context.camera;

  // Rotation.
  if (mouse.right) {
    camera.rotate(-mouse.position.dx * rotation_speed, camera.up());
    camera.rotate(-mouse.position.dy * rotation_speed, camera.right());
  }

  // Translation.
  const auto shift_modifier = keyboard.shift ? 10.0f : 1.0f;
  const auto alt_modifier = keyboard.alt ? 0.2f : 1.0f;

  if (keyboard.w) {
    camera.translate(-camera.forward() * translation_speed * shift_modifier *
                     alt_modifier);
  }

  if (keyboard.s) {
    camera.translate(camera.forward() * translation_speed * shift_modifier *
                     alt_modifier);
  }

  if (keyboard.a) {
    camera.translate(-camera.right() * translation_speed * shift_modifier *
                     alt_modifier);
  }

  if (keyboard.d) {
    camera.translate(camera.right() * translation_speed * shift_modifier *
                     alt_modifier);
  }
}

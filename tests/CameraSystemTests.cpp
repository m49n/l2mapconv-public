#include "CameraSystem.h"
#include "TestSupport.h"

#include <algorithm>
#include <cmath>

int run_camera_system_tests() {
  int failures = 0;
  for (const float sign : {-1.f, 1.f}) {
    RenderingContext rendering;
    WindowContext window{};
    UIContext ui{};
    CameraSystem controls{rendering, window, ui};
    auto &camera = rendering.camera;
    const auto pitch = [&] {
      return glm::degrees(std::asin(
          std::clamp(glm::dot(camera.forward(), camera.up()), -1.f, 1.f)));
    };
    const auto upright = [&] {
      return glm::dot(glm::cross(camera.right(), camera.forward()),
                      camera.up()) > 0.f;
    };
    const auto look = [&](float yaw_deg, float pitch_deg) {
      window.mouse.position.dx =
          -glm::radians(yaw_deg) / ui.camera.mouse_sensitivity;
      window.mouse.position.dy =
          -glm::radians(pitch_deg) / ui.camera.mouse_sensitivity;
      controls.frame_begin(Timestep{.016f});
    };

    // Mouse movement without RMB must not rotate the camera.
    const auto initial = camera.forward();
    look(25, 35);
    failures += expect(glm::length(camera.forward() - initial) < .0001f,
                       "mouse look requires the right button");
    window.mouse.right = true;
    look(0, sign * 180);
    failures += expect(
        std::abs(pitch() - sign * 89.f) < .02f && upright(),
        "large vertical mouse delta stops before flipping at either pole");

    bool stayed_upright = true;
    for (int i = 0; i < 100; ++i) {
      look(0, sign * 180);
      stayed_upright &= upright() && std::abs(pitch() - sign * 89.f) < .02f;
    }
    failures += expect(
        stayed_upright,
        "continued vertical input cannot rotate through the pitch limit");
    look(0, -sign * 10);
    failures += expect(std::abs(pitch() - sign * 79.f) < .02f && upright(),
                       "reversing at the limit responds immediately without "
                       "accumulated overshoot");

    const auto before_yaw = camera.forward();
    look(90, 0);
    const auto horizontal_before = glm::normalize(glm::vec2(before_yaw));
    const auto horizontal_after = glm::normalize(glm::vec2(camera.forward()));
    failures += expect(
        std::abs(glm::dot(horizontal_before, horizontal_after)) < .001f &&
            std::abs(pitch() - sign * 79.f) < .02f && upright(),
        "horizontal look still turns at steep pitch without rolling");
    for (int i = 0; i < 7; ++i)
      look(90, 0);
    failures +=
        expect(glm::length(camera.forward() - before_yaw) < .001f && upright(),
               "horizontal rotation stays unrestricted across full turns");
    failures += expect(glm::length(camera.position()) < .0001f,
                       "mouse look never translates the camera");
  }
  return failures;
}

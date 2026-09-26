#pragma once

#include "LiveSceneSettings.h"

#include <rendering/Camera.h>
#include <rendering/Context.h>
#include <rendering/Scene.h>
#include <territory/VisualScene.h>

#include <memory>

class LiveVisualRenderer {
public:
  explicit LiveVisualRenderer(rendering::Context &context,
                              int max_shadow_texture_size_for_test = 0);
  ~LiveVisualRenderer();
  LiveVisualRenderer(const LiveVisualRenderer &) = delete;
  LiveVisualRenderer &operator=(const LiveVisualRenderer &) = delete;

  void upload(rendering::SceneGroupId group, const territory::VisualScene &scene);
  void remove(rendering::SceneGroupId group);
  void render(const rendering::Camera &camera, const LiveSceneSettings &settings,
              LiveSceneDiagnostics &diagnostics);

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

#pragma once

#include "Camera.h"
#include "Context.h"
#include "Entity.h"
#include "EntityShader.h"
#include "EntityTree.h"
#include "FrameSettings.h"

#include <utils/NonCopyable.h>

#include <vector>

namespace rendering {

class Renderer : public utils::NonCopyable {
public:
  explicit Renderer(Context &context, const Camera &camera);

  void resize(int width, int height) const;
  void clear() const;

  void render(const EntityTree &tree, const FrameSettings &settings,
              int &draws) const;

private:
  Context &m_context;
  const Camera &m_camera;

  void unbind_current_texture() const;
};

} // namespace rendering

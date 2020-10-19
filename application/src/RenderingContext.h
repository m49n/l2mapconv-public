#pragma once

#include <rendering/Camera.h>
#include <rendering/Context.h>
#include <rendering/EntityTree.h>

struct RenderingContext {
  rendering::Context context;
  rendering::Camera camera;

  rendering::EntityTree entity_tree;

  RenderingContext()
      : context{}, camera{context, 45.0f, 50.0f, {0.0f, 0.0f, 0.0f}} {}
};

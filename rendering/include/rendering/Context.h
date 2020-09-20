#pragma once

namespace rendering {

struct Context {
  struct {
    struct {
      int width;
      int height;
    } size;
  } framebuffer;

  struct {
    unsigned int vao;
  } mesh;

  struct {
    unsigned int program;
  } shader;

  struct {
    unsigned int texture;
  } texture;
};

} // namespace rendering

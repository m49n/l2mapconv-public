#pragma once

#include <GLFW/glfw3.h>

struct WindowContext {
  struct {
    struct Size {
      int width;
      int height;
    } size;
  } framebuffer;

  struct Window {
    struct {
      int width;
      int height;
    } size;
  } window;

  struct Mouse {
    struct {
      double x;
      double y;
      float dx;
      float dy;
    } position;

    bool right;
  } mouse;

  struct {
    bool w, a, s, d;
    bool m;
    bool shift;
    bool alt;
  } keyboard;

  GLFWwindow *window_handle;
};

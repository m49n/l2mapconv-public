#include "pch.h"

#include "WindowSystem.h"

WindowSystem::WindowSystem(WindowContext &window_context,
                           ApplicationContext &application_context,
                           UIContext &ui_context, const std::string &title,
                           int width, int height)
    : m_window_context{window_context},
      m_application_context{application_context},
      m_ui_context{ui_context}, m_window{create_window(title, width, height)} {

  glfwSetWindowUserPointer(m_window, &m_window_context);

  glfwSetKeyCallback(m_window, [](GLFWwindow *window, int key, int /*scancode*/,
                                  int action, int /*mods*/) {
    auto *context =
        static_cast<WindowContext *>(glfwGetWindowUserPointer(window));

    context->keyboard.m = key == GLFW_KEY_M && action == GLFW_PRESS;
  });
}

WindowSystem::~WindowSystem() { glfwTerminate(); }

void WindowSystem::start() {
  glfwGetCursorPos(m_window, &m_window_context.mouse.position.x,
                   &m_window_context.mouse.position.y);
}

void WindowSystem::frame_begin(Timestep /*frame_time*/) {
  glfwPollEvents();

  // Window.
  glfwGetWindowSize(m_window, &m_window_context.window.size.width,
                    &m_window_context.window.size.height);

  // Framebuffer.
  glfwGetFramebufferSize(m_window, &m_window_context.framebuffer.size.width,
                         &m_window_context.framebuffer.size.height);

  // Mouse.
  m_window_context.mouse.right =
      glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

  const auto old_mouse_position = m_window_context.mouse.position;

  glfwGetCursorPos(m_window, &m_window_context.mouse.position.x,
                   &m_window_context.mouse.position.y);

  if (m_window_context.mouse.right) {
    m_window_context.mouse.position.dx = static_cast<float>(
        old_mouse_position.x - m_window_context.mouse.position.x);
    m_window_context.mouse.position.dy = static_cast<float>(
        old_mouse_position.y - m_window_context.mouse.position.y);
  }

  if (m_window_context.mouse.right) {
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  } else {
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }

  // Keyboard.
  m_window_context.keyboard.w = glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS;
  m_window_context.keyboard.a = glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS;
  m_window_context.keyboard.s = glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS;
  m_window_context.keyboard.d = glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS;

  m_window_context.keyboard.shift =
      glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

  m_window_context.keyboard.alt =
      glfwGetKey(m_window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;

  // Exit on Esc.
  if (glfwWindowShouldClose(m_window) != 0 ||
      glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {

    m_application_context.running = false;
  }
}

void WindowSystem::frame_end(Timestep /*frame_time*/) {
  m_window_context.keyboard = {};

  //  GL_CALL(glFlush());
  glfwSwapBuffers(m_window);
}

auto WindowSystem::create_window(const std::string &title, int width,
                                 int height) const -> GLFWwindow * {

  ASSERT(glfwInit() == GLFW_TRUE, "App", "Can't initialize GLFW");

  // OpenGL context.
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  // Window properties.
  glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
  //  glfwWindowHint(GLFW_DOUBLEBUFFER, GL_FALSE);
  glfwWindowHint(GLFW_SAMPLES, 4);

  // Create window.
  auto *window =
      glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
  ASSERT(window != nullptr, "App", "Can't create window");

  glfwMakeContextCurrent(window);

  m_window_context.window_handle = window;
  return window;
}

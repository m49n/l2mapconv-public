#include "WindowSystem.h"
#include "TestSupport.h"

// Replace only the external GLFW/OS boundary. The real WindowSystem is linked
// below, so these tests also run without a desktop or graphics driver.
struct GLFWwindow {
  void *user{};
  int cursor{GLFW_CURSOR_NORMAL};
  bool raw{};
};

namespace {
GLFWwindow test_window;
GLFWwindow *current{};
bool raw_supported{}, right_down{};
int swap_interval{}, platform_errors{};

int check_startup(bool supported, int initial_interval) {
  test_window = {};
  current = nullptr;
  raw_supported = supported;
  right_down = false;
  swap_interval = initial_interval;
  platform_errors = 0;

  WindowContext context{};
  ApplicationContext application{};
  WindowSystem window(context, application, "test", 1440, 1000);
  auto failures = expect(current == context.window_handle,
                         "viewer makes its own context current");
  failures += expect(swap_interval == 1,
                     "viewer enables VSync regardless of driver default");
  failures += expect(test_window.raw == supported,
                     "viewer requests raw mouse motion only when supported");
  failures += expect(platform_errors == 0,
                     "unsupported raw input is not requested");
  failures += expect(test_window.cursor == GLFW_CURSOR_NORMAL,
                     "startup leaves the UI cursor uncaptured");

  window.start();
  right_down = true;
  window.frame_begin(Timestep{0.016f});
  failures += expect(test_window.cursor == GLFW_CURSOR_DISABLED &&
                         test_window.raw == supported,
                     "RMB captures the cursor with the selected input mode");
  right_down = false;
  window.frame_begin(Timestep{0.016f});
  failures += expect(test_window.cursor == GLFW_CURSOR_NORMAL,
                     "releasing RMB restores the UI cursor");
  return failures;
}
} // namespace

// This fake models the API preconditions relevant to window initialization,
// not native input delivery, which is covered by the separate manual test.
int glfwInit() { return GLFW_TRUE; }
void glfwTerminate() { current = nullptr; }
void glfwWindowHint(int, int) {}
GLFWwindow *glfwCreateWindow(int, int, const char *, GLFWmonitor *, GLFWwindow *) {
  return &test_window;
}
void glfwMakeContextCurrent(GLFWwindow *window) { current = window; }
void glfwSwapInterval(int interval) {
  if (!current) {
    ++platform_errors;
    return;
  }
  swap_interval = interval;
}
int glfwRawMouseMotionSupported() { return raw_supported; }
void glfwSetWindowUserPointer(GLFWwindow *window, void *value) {
  window->user = value;
}
void *glfwGetWindowUserPointer(GLFWwindow *window) { return window->user; }
GLFWkeyfun glfwSetKeyCallback(GLFWwindow *, GLFWkeyfun) { return nullptr; }
void glfwGetCursorPos(GLFWwindow *, double *x, double *y) { *x = *y = 0.; }
void glfwPollEvents() {}
void glfwGetWindowSize(GLFWwindow *, int *width, int *height) {
  *width = 1440;
  *height = 1000;
}
void glfwGetFramebufferSize(GLFWwindow *window, int *width, int *height) {
  glfwGetWindowSize(window, width, height);
}
int glfwGetMouseButton(GLFWwindow *, int button) {
  return button == GLFW_MOUSE_BUTTON_RIGHT && right_down ? GLFW_PRESS
                                                        : GLFW_RELEASE;
}
void glfwSetInputMode(GLFWwindow *window, int mode, int value) {
  if (mode == GLFW_CURSOR) {
    window->cursor = value;
  } else if (mode == GLFW_RAW_MOUSE_MOTION && raw_supported) {
    window->raw = value == GLFW_TRUE;
  } else {
    ++platform_errors;
  }
}
int glfwGetKey(GLFWwindow *, int) { return GLFW_RELEASE; }
int glfwWindowShouldClose(GLFWwindow *) { return GLFW_FALSE; }
void glfwSwapBuffers(GLFWwindow *) {}

int main() {
  const auto failures = check_startup(true, 0) + check_startup(false, 0) +
                        check_startup(true, -1);
  return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}

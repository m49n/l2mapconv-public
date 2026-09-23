#include "GLResources.h"
#include <vector>
namespace territory::gl {
void check(const char *where) {
  auto error = glGetError();
  if (error != GL_NO_ERROR)
    throw std::runtime_error(std::string(where) + ": OpenGL error " +
                             std::to_string(error));
}
namespace {
Handle compile(GLenum type, const std::string &source) {
  Handle shader(Kind::Shader, glCreateShader(type));
  const char *text = source.c_str();
  glShaderSource(shader, 1, &text, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    GLint n = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &n);
    std::vector<char> log(std::max(n, 1));
    glGetShaderInfoLog(shader, n, nullptr, log.data());
    throw std::runtime_error("GLSL compile: " + std::string(log.data()));
  }
  return shader;
}
} // namespace
Handle program(const std::string &vertex, const std::string &fragment) {
  auto vs = compile(GL_VERTEX_SHADER, vertex),
       fs = compile(GL_FRAGMENT_SHADER, fragment);
  Handle p(Kind::Program);
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    GLint n = 0;
    glGetProgramiv(p, GL_INFO_LOG_LENGTH, &n);
    std::vector<char> log(std::max(n, 1));
    glGetProgramInfoLog(p, n, nullptr, log.data());
    throw std::runtime_error("GLSL link: " + std::string(log.data()));
  }
  return p;
}
} // namespace territory::gl

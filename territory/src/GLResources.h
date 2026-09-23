#pragma once
#include <GL/glew.h>
#include <stdexcept>
#include <string>
#include <utility>
namespace territory::gl {
enum class Kind {
  Texture,
  Buffer,
  VertexArray,
  Framebuffer,
  Renderbuffer,
  Program,
  Shader
};
class Handle {
public:
  Handle(Kind kind, GLuint value) : kind(kind), value(value) {}
  explicit Handle(Kind kind) : kind(kind) {
    switch (kind) {
    case Kind::Texture:
      glGenTextures(1, &value);
      break;
    case Kind::Buffer:
      glGenBuffers(1, &value);
      break;
    case Kind::VertexArray:
      glGenVertexArrays(1, &value);
      break;
    case Kind::Framebuffer:
      glGenFramebuffers(1, &value);
      break;
    case Kind::Renderbuffer:
      glGenRenderbuffers(1, &value);
      break;
    case Kind::Program:
      value = glCreateProgram();
      break;
    case Kind::Shader:
      throw std::logic_error("Shader needs a type");
    }
    if (!value)
      throw std::runtime_error("OpenGL resource allocation failed");
  }
  ~Handle() { reset(); }
  Handle(const Handle &) = delete;
  Handle(Handle &&other) noexcept
      : kind(other.kind), value(std::exchange(other.value, 0)) {}
  Handle &operator=(Handle &&other) noexcept {
    if (this != &other) {
      reset();
      kind = other.kind;
      value = std::exchange(other.value, 0);
    }
    return *this;
  }
  operator GLuint() const { return value; }

private:
  Kind kind;
  GLuint value{};
  void reset() {
    if (!value)
      return;
    switch (kind) {
    case Kind::Texture:
      glDeleteTextures(1, &value);
      break;
    case Kind::Buffer:
      glDeleteBuffers(1, &value);
      break;
    case Kind::VertexArray:
      glDeleteVertexArrays(1, &value);
      break;
    case Kind::Framebuffer:
      glDeleteFramebuffers(1, &value);
      break;
    case Kind::Renderbuffer:
      glDeleteRenderbuffers(1, &value);
      break;
    case Kind::Program:
      glDeleteProgram(value);
      break;
    case Kind::Shader:
      glDeleteShader(value);
      break;
    }
    value = 0;
  }
};
void check(const char *where);
Handle program(const std::string &vertex, const std::string &fragment);
} // namespace territory::gl

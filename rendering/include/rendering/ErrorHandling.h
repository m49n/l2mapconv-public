#pragma once

#include <GL/glew.h>
#include <utils/Assert.h>

#ifdef NDEBUG
#define GL_CALL(function) function;
#else
#define GL_CALL(function)                                                      \
  while (glGetError() != GL_NO_ERROR)                                          \
    ;                                                                          \
  function;                                                                    \
  {                                                                            \
    const auto error_code = glGetError();                                      \
    ASSERT(error_code == GL_NO_ERROR, "Rendering",                             \
           "OpenGL error in " << #function << ": " << error_code)              \
  }
#endif

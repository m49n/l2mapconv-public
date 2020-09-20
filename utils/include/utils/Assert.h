#pragma once

#include "Log.h"

#include <cstdlib>
#include <ostream>

#if _WIN32
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK()
#endif

#ifdef NDEBUG
#define DEBUG_ABORT()
#else
#define DEBUG_ABORT()                                                          \
  do {                                                                         \
    DEBUG_BREAK();                                                             \
    abort();                                                                   \
  } while (false);
#endif

#define ASSERT_3_ARGS(condition, space, message)                               \
  do {                                                                         \
    if (!(condition)) {                                                        \
      utils::Log(utils::LOG_ERROR, space)                                      \
          << "Assertion (" << #condition << ") failed at " << __FILE__ << "("  \
          << __LINE__ << "): " << message << std::endl;                        \
      DEBUG_ABORT();                                                           \
    }                                                                          \
  } while (false);

#define ASSERT_2_ARGS(condition, message) ASSERT_3_ARGS(condition, "", message)
#define ASSERT_4TH_ARG(arg1, arg2, arg3, arg4, ...) arg4
#define ASSERT_MACRO_CHOOSER(...)                                              \
  ASSERT_4TH_ARG(__VA_ARGS__, ASSERT_3_ARGS, ASSERT_2_ARGS, ASSERT_2_ARGS)

#define ASSERT(...) ASSERT_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

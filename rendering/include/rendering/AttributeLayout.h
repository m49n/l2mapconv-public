#pragma once

namespace rendering {

struct FloatAttributeLayout {
  unsigned int index;
  int size;
  unsigned int type;
  unsigned char normalized;
  int stride;
  const void *pointer;
  unsigned int divisor;
};

struct IntAttributeLayout {
  unsigned int index;
  int size;
  unsigned int type;
  int stride;
  const void *pointer;
  unsigned int divisor;
};

} // namespace rendering

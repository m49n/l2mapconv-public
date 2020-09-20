#pragma once

namespace rendering {

struct AttributeLayout {
  unsigned int index;
  int size;
  unsigned int type;
  unsigned char normalized;
  int stride;
  const void *pointer;
};

} // namespace rendering
